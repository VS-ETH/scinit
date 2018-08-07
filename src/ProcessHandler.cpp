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

#include "ProcessHandler.h"
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <csignal>
#include <iostream>
#include <mutex>
#include "ChildProcess.h"
#include "ChildProcessException.h"
#include "Config.h"
#include "ProcessHandlerException.h"
#include "log.h"

#define MAX_EVENTS 10
#define BUF_SIZE 4096

namespace scinit {
    void ProcessHandler::register_processes(std::list<std::weak_ptr<ChildProcessInterface>>& refs) {
        this->all_objs = refs;
    }

    void ProcessHandler::register_for_process_state(
      int id, std::function<void(ProcessHandlerInterface::ProcessEvent, int)> handler) {
        if (sig_for_id.count(id) == 0) {
            sig_for_id.insert(std::make_pair(
              id, std::make_shared<boost::signals2::signal<void(ProcessHandlerInterface::ProcessEvent, int)>>()));
        }
        auto signal = sig_for_id[id];
        signal->connect(handler);
    }

    void ProcessHandler::register_obj_id(int id, std::weak_ptr<ChildProcessInterface> obj) {
        obj_for_id[id] = std::move(obj);
    }

    void ProcessHandler::signal_received(unsigned int signal) {
        if (signal == SIGCHLD) {
            // Already handled with waitpid
        } else {
            // Forward signal
            for (auto pair : id_for_pid) {
                LOG->debug("Forwarding signal {0} to pid {1}", signal, pair.first);
                kill(pair.first, signal);
            }

            // NOLINTNEXTLINE(hicpp-signed-bitwise)
            if ((signal & SIGTERM) || (signal & SIGQUIT)) {
                LOG->warn("Received SIGTERM/SIGQUIT, stopping all children");
                should_quit = true;
            }
        }
    }

    void ProcessHandler::handle_child_output(int fd, const std::string& str) {
        auto id = id_for_fd.at(fd);
        if (auto obj = obj_for_id.at(id).lock()) {
            std::string name = obj->get_name();
            if (!fd_type.count(fd)) {
                LOG->critical(
                  "BUG: Child (id {0}) outputted something from a file descriptor we don't know the type of!", id);
            } else {
                switch (fd_type[fd]) {
                    case FDType::STDOUT:
                        spdlog::get(name)->info(str);
                        break;
                    case FDType::STDERR:
                        spdlog::get(name)->warn(str);
                        break;
                }
            }
        } else {
            LOG->critical("BUG: Child (id {0}) outputted something but the object has already been freed!", id);
        }
    }

    void ProcessHandler::event_received(int fd, unsigned int event) {
        if (event & EPOLLIN) {
            if (fd == signal_fd) {
                // None of our children, this is a signal
                struct signalfd_siginfo signal = {};
                auto size_read = read(signal_fd, &signal, sizeof(struct signalfd_siginfo));
                if (size_read != sizeof(struct signalfd_siginfo)) {
                    LOG->critical("Signal buffer read didn't match expected size, aborting!");
                    throw ProcessHandlerException();
                }

                signal_received(signal.ssi_signo);
            } else {
                char buf[BUF_SIZE + 1] = {0};
                ssize_t nchars = read(fd, &buf, BUF_SIZE);
                auto str = std::string(static_cast<char*>(buf));

                // Strip leading and trailing newlines
                ssize_t begin = 0;
                for (; begin < nchars; begin++) {
                    if (buf[begin] != '\n') {
                        break;
                    }
                }
                ssize_t end = nchars - 1;
                for (; end > begin; end--) {
                    if (buf[end] != '\n') {
                        break;
                    }
                }
                if (begin > 0) {
                    str = str.substr(static_cast<uint64_t>(begin));
                }
                if (end < nchars - 1 && end >= begin && begin >= 0) {
                    str = str.substr(0, static_cast<uint64_t>(end - begin + 1));
                }

                // If there is something left, output it
                if (!str.empty()) {
                    handle_child_output(fd, str);
                }
            }
        } else if (event & EPOLLHUP) {
            LOG->debug("Child process has closed control terminal (SIGHUP)!");
            struct epoll_event event_buf {};
            event_buf.data.fd = fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event_buf) == -1) {
                LOG->critical("Couldn't remove child file descriptor from epoll, aborting!");
                throw ProcessHandlerException();
            }
        } else {
            LOG->critical("unknown event type ({0}), aborting!", event);
            throw ProcessHandlerException();
        }
    }

    void ProcessHandler::sigchld_received(int pid, int rc) {
        if (id_for_pid.count(pid) > 0) {
            // One of ours!
            int id = id_for_pid[pid];
            if (auto ptr = obj_for_id[id].lock()) {
                if (rc == 0) {
                    LOG->info("Child {0} (PID {1}) exitted with RC {2}", ptr->get_name(), pid, rc);
                } else {
                    LOG->warn("Child {0} (PID {1}) exitted with RC {2}", ptr->get_name(), pid, rc);
                }
            } else {
                LOG->critical("BUG: Child (PID {0}) exitted with RC {1} and the object has already been freed!", pid,
                              rc);
            }
            number_of_running_procs--;
            (*sig_for_id[id])(EXIT, rc);
        } else {
            LOG->info("Reaped zombie (PID {0}) with RC {1}", pid, rc);
        }
    }

    int ProcessHandler::enter_eventloop() {
        setup_signal_handlers();
        for (auto& child : all_objs) {
            if (auto ptr = child.lock()) {
                ptr->notify_of_state(obj_for_id);
            } else {
                LOG->warn("Free'd child in child list!");
            }
        }

        start_programs();

        // Everything is set up, now we only need to wait for events
        LOG->debug("Entering main event loop");
        struct epoll_event events[MAX_EVENTS];
        while (true) {
            int num_fds = epoll_wait(epoll_fd, static_cast<epoll_event*>(events), MAX_EVENTS, 1000);
            LOG->debug("Epoll got back, num_fds: {0}", num_fds);

            // Go look for children and zombies
            int rc, pid;
            while ((pid = waitpid(-1, &rc, WNOHANG)) > 0) {
                sigchld_received(pid, rc);
            }

            if (num_fds > 0) {
                for (int i = 0; i < num_fds; i++) {
                    auto event = events[i];
                    int eventnum = event.events;
                    LOG->debug("Event: {0}", eventnum);
                    event_received(event.data.fd, event.events);
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

    // TODO(uubk): Refactor error handling
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
        struct epoll_event setup {};
        setup.data.fd = signal_fd;
        setup.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &setup) == -1) {
            LOG->critical("Couldn't add signalfd to epoll socket, aborting!");
            throw ProcessHandlerException();
        }
    }

    void ProcessHandler::start_programs() {
        for (const auto& weak_program : all_objs) {
            if (auto program = weak_program.lock()) {
                if (!program->can_start_now()) {
                    continue;
                }
                try {
                    // Register logger
                    auto console = spdlog::stdout_color_st(program->get_name());
                    console->set_pattern("[%^%n%$] [%H:%M:%S.%e] %v");

                    // Start program
                    LOG->info("Starting: {0}", program->get_name());
                    program->do_fork(id_for_pid);
                    program->register_with_epoll(epoll_fd, id_for_fd, fd_type);
                    number_of_running_procs++;
                } catch (std::exception& e) { LOG->critical("Couldn't start program: {0}", e.what()); }
            } else {
                LOG->warn("Free'd child in child list!");
            }
        }
    }
}  // namespace scinit