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

#define MAX_EVENTS 10
#define BUF_SIZE 4096

int main(int argc, char** argv) {
    auto conf = scinit::handle_commandline_invocation(argc, argv);
    auto procs = conf->getProcesses();
    std::map<int, std::shared_ptr<scinit::ChildProcess>> fd_to_object;
    std::map<int, std::shared_ptr<scinit::ChildProcess>> pid_to_object;
    std::list<std::shared_ptr<scinit::ChildProcess>> processes_to_restart;
    int number_of_running_procs = 0;
    bool should_quit = false;

    // Instead of using signal handlers, use signalfd as we're using epoll anyways
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        LOG->critical("Couldn't block signals from executing their default handlers, aborting!");
        return -1;
    }
    int signal_fd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (signal_fd == -1) {
        LOG->critical("Couldn't create signalfd, aborting!");
        return -1;
    }

    struct epoll_event events[MAX_EVENTS];
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        LOG->critical("Couldn't create epoll socket, aborting!");
        return -1;
    }
    struct epoll_event setup{};
    setup.data.fd = signal_fd;
    setup.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &setup) == -1) {
        LOG->critical("Couldn't add signalfd to epoll socket, aborting!");
        return -1;
    }

    LOG->info("Starting programs");
    for (const auto & program : procs) {
        try {
            // Register logger
            auto console = spdlog::stdout_color_st(program->get_name());
            console->set_pattern("[%^%n%$] [%H:%M:%S.%e] %v");

            // Start program
            program->do_fork(pid_to_object);
            program->register_with_epoll(epoll_fd, fd_to_object);
            number_of_running_procs++;
        } catch (std::exception & e) {
            LOG->critical("Couldn't start program: {0}", e.what());
        }
    }

    // Everything is set up, now we only need to wait for events
    LOG->debug("Entering main event loop");
    while (true) {
        int num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        LOG->debug("Epoll got back, num_fds: {0}", num_fds);

        // Go look for children and zombies
        int rc, pid;
        while ((pid = waitpid(-1, &rc, WNOHANG)) > 0 ) {
            if (pid_to_object.count(pid) > 0) {
                // One of ours!
                LOG->info("Child {0} (PID {1}) exitted with RC {2}", pid_to_object[pid]->get_name(), pid, rc);

                number_of_running_procs--;

                pid_to_object[pid]->notify_of_exit(rc);
                if (pid_to_object[pid]->should_restart()) {
                    auto ptr = pid_to_object[pid];
                    processes_to_restart.push_back(ptr);
                }
                pid_to_object.erase(pid);

                if (number_of_running_procs == 0 && (processes_to_restart.empty() || should_quit)) {
                    LOG->info("Last running process exitted and no process left to restart, exiting program");
                    return 0;
                }
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
                           for (auto pair : pid_to_object) {
                               kill(pair.first, signal.ssi_signo);
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
                        if (str.size() > 0)
                            spdlog::get(fd_to_object[event.data.fd]->get_name())->info(str);
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

        // Are there processes to restart?
        if (!should_quit) {
            std::list<std::shared_ptr<scinit::ChildProcess>> processes_restarted;
            for (auto &program : processes_to_restart) {
                try {
                    if (program->should_restart_now()) {
                        program->do_fork(pid_to_object);
                        program->register_with_epoll(epoll_fd, fd_to_object);
                        number_of_running_procs++;
                        processes_restarted.push_back(program);
                    }
                } catch (scinit::ChildProcessException &error) {
                    LOG->critical("Couldn't restart process: {0}, aborting!", error.what());
                    return -1;
                }
            }
            for (auto &program : processes_restarted) {
                processes_to_restart.remove(program);
            }
        }
    }

    return 0;
}