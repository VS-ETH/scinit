/*
 * Copyright 2018 The scinit authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <mutex>
#include <csignal>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include "Config.h"
#include "log.h"
#include "ChildProcessException.h"
#include "ProcessHandler.h"
#include "ProcessHandlerException.h"
#include "ChildProcess.h"

#define MAX_EVENTS 10
#define BUF_SIZE 4096

namespace scinit {
    void ProcessHandler::register_processes(std::list<std::shared_ptr<ChildProcessInterface>>& refs) {
        this->all_objs = refs;
    }

    void ProcessHandler::register_for_process_state(int id,
                                            std::function<void(ProcessHandlerInterface::ProcessEvent, int)> handler) {
        if (sig_for_id.count(id) == 0)
            sig_for_id.insert(std::make_pair(id,
                                new boost::signals2::signal<void(ProcessHandlerInterface::ProcessEvent, int)>()));
        auto signal = sig_for_id[id];
        signal->connect(handler);
    }

    void ProcessHandler::register_obj_id(int id, std::shared_ptr<ChildProcessInterface> obj) {
        obj_for_id[id] = std::move(obj);
    }

    int ProcessHandler::enter_eventloop() {
        int number_of_running_procs = 0;
        bool should_quit = false;

        setup_signal_handlers();
        for (auto& child: all_objs) {
            child->notify_of_state(all_objs);
        }

        start_programs();

        // Everything is set up, now we only need to wait for events
        LOG->debug("Entering main event loop");
        struct epoll_event events[MAX_EVENTS];
        while (true) {
            int num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
            LOG->debug("Epoll got back, num_fds: {0}", num_fds);

            // Go look for children and zombies
            int rc, pid;
            while ((pid = waitpid(-1, &rc, WNOHANG)) > 0 ) {
                if (id_for_pid.count(pid) > 0) {
                    // One of ours!
                    int id = id_for_pid[pid];
                    LOG->info("Child {0} (PID {1}) exitted with RC {2}",
                              obj_for_id[id]->get_name(), pid, rc);

                    number_of_running_procs--;
                    (*sig_for_id[id])(EXIT, rc);
                } else {
                    LOG->info("Reaped zombie (PID {0}) with RC {1}", pid, rc);
                }
            }

            if (num_fds > 0) {
                for (int i = 0; i < num_fds; i++) {
                    auto event = events[i];
                    int eventnum = event.events;
                    LOG->debug("Event: {0}", eventnum);
                    if (event.events & EPOLLIN) {
                        if (event.data.fd == signal_fd) {
                            // None of our children, this is a signal
                            struct signalfd_siginfo signal = {};
                            auto size_read = read(signal_fd, &signal, sizeof(struct signalfd_siginfo));
                            if (size_read != sizeof(struct signalfd_siginfo)) {
                                LOG->critical("Signal buffer read didn't match expected size, aborting!");
                                return -1;
                            }

                            if (signal.ssi_signo == SIGCHLD) {
                                // Already handled with waitpid above
                            } else {
                                // Forward signal
                                for (auto pair : id_for_pid) {
                                    kill(pair.second, signal.ssi_signo);
                                }

                                if ((signal.ssi_signo & SIGTERM) || (signal.ssi_signo & SIGQUIT)) {
                                    LOG->warn("Received SIGTERM/SIGQUIT, stopping all children");
                                    should_quit = true;
                                }
                            }
                        } else {
                            char buf[BUF_SIZE+1] = {0};
                            auto nchars = read(event.data.fd, &buf, BUF_SIZE);
                            auto str = std::string(buf);

                            // Strip leading and trailing newlines
                            size_t begin=0;
                            for (; begin<nchars; begin++)
                                if (buf[begin]!='\n')
                                    break;
                            size_t end=nchars-1;
                            for (; end>begin; end--)
                                if (buf[end]!='\n')
                                    break;
                            if (begin > 0)
                                str = str.substr(begin);
                            if (end < nchars-1 && end >= begin && begin >= 0)
                                str = str.substr(0, end-begin+1);

                            // If there is something left, output it
                            if (str.size() > 0) {
                                int id = id_for_fd.at(event.data.fd);
                                auto obj = obj_for_id.at(id);
                                std::string name = obj->get_name();
                                spdlog::get(name)->info(str);
                            }
                        }
                    } else if (event.events & EPOLLHUP) {
                        LOG->info("Child process has closed control terminal (SIGHUP)!");
                        event.events = 0;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event.data.fd, &event) == -1) {
                            LOG->critical("Couldn't remove child file descriptor from epoll, aborting!");
                            return -1;
                        }
                    } else {
                        LOG->critical("unknown event type ({0}), aborting!", eventnum);
                        return -1;
                    }
                }
            }

            if (number_of_running_procs == 0 && should_quit) {
                LOG->info("Last running process exitted and we're supposed to quit, exiting program");
                return 0;
            }

            if (!should_quit) {
                start_programs();
                if (number_of_running_procs == 0) {
                    LOG->info("Last running process exitted and no process left to restart, exiting program");
                    return 0;
                }
            }
        }
    }

    // TODO: Refactor error handling
    void ProcessHandler::setup_signal_handlers() noexcept(false) {
        // Instead of using signal handlers, use signalfd as we're using epoll anyways
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGQUIT);
        if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
            LOG->critical("Couldn't block signals from executing their default handlers, aborting!");
            throw ProcessHandlerException();
        }
        signal_fd = signalfd(-1, &mask, SFD_CLOEXEC);
        if (signal_fd == -1) {
            LOG->critical("Couldn't create signalfd, aborting!");
            throw ProcessHandlerException();
        }

        epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd == -1) {
            LOG->critical("Couldn't create epoll socket, aborting!");
            throw ProcessHandlerException();
        }
        struct epoll_event setup{};
        setup.data.fd = signal_fd;
        setup.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &setup) == -1) {
            LOG->critical("Couldn't add signalfd to epoll socket, aborting!");
            throw ProcessHandlerException();
        }
    }

    void ProcessHandler::start_programs() {
        for (const auto & program : all_objs) {
            if (!program->can_start_now())
                continue;
            try {
                // Register logger
                auto console = spdlog::stdout_color_st(program->get_name());
                console->set_pattern("[%^%n%$] [%H:%M:%S.%e] %v");

                // Start program
                LOG->info("Starting: {0}", program->get_name());
                program->do_fork(id_for_pid);
                program->register_with_epoll(epoll_fd, id_for_fd);
                number_of_running_procs++;
            } catch (std::exception & e) {
                LOG->critical("Couldn't start program: {0}", e.what());
            }
        }
    }
}