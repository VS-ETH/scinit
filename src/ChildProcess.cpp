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

#include <unistd.h>
#include <fcntl.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/epoll.h>
#include <iostream>
#include <memory>
#include <cstring>
#include "log.h"
#include "ChildProcess.h"
#include "ChildProcessException.h"

namespace scinit {
    ChildProcess::ChildProcess(const std::string &name, const std::string &path, const std::list<std::string>& args,
                               const std::string &type, const std::list<std::string> &capabilities, unsigned int uid,
                               unsigned int gid, unsigned int graph_id,
                               std::shared_ptr<ProcessHandlerInterface> handler) : path(path), args(args), name(name),
                                                                                   capabilities(capabilities), uid(uid),
                                                                                   gid(gid), graph_id(graph_id),
                                                                                   handler(handler) {
        this->state = READY;

        if (type == "simple") {
            this->type = SIMPLE;
        } else {
            this->type = ONESHOT;
        }

        auto ref = std::bind(&ChildProcess::handle_process_event, this,
                std::placeholders::_1, std::placeholders::_2);
        handler->register_for_process_state(graph_id, ref);
    }

    unsigned int ChildProcess::get_id() const noexcept {
        return graph_id;
    }

    void ChildProcess::handle_caps() {
        // Step 1: Make sure that the this process has all caps that we need
        std::string cap_string = "CAP_SETUID=eip CAP_SETGID=eip CAP_SETPCAP=eip";
        cap_t new_caps_transitional;
        for (const auto & capability: capabilities)
            cap_string += " "+capability+"=eip";
        const char* c_cap_string = cap_string.c_str();
        new_caps_transitional = cap_from_text(c_cap_string);

        int rc = cap_set_proc(new_caps_transitional);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities, some features might not work as intended!" << std::endl;
        }

        // Keep caps across setuid because otherwise we can't set ambient caps below
        prctl(PR_SET_KEEPCAPS, 1);
        // Set user and group
        if (setgid(this->gid) == -1)
            std::cerr << "Couldn't set group!" << std::endl;
        if (setuid(this->uid) == -1)
            std::cerr << "Couldn't set user!" << std::endl;
        prctl(PR_SET_KEEPCAPS, 0);

        rc = cap_set_proc(new_caps_transitional);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities, some features might not work as intended!" << std::endl;
        }

        // Step 2: Make sure that the child process only retains the capabilities explicitly specified
        // Clear all ambient capabielities and
        prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
        // ... re-add those we need explicitly
        cap_string = "";
        for (const auto & capability: capabilities) {
            cap_string += " "+capability+"=eip";
            cap_value_t cap = {};
            if (cap_from_name(capability.c_str(), &cap) != 0) {
                std::cerr << "Warning: Couldn't decode '" << capability << "', not adding to set." << std::endl;
            }
            if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) != -1) {
                std::cout << "Ambient capability '" << capability << "' enabled." << std::endl;
            } else {
                std::cout << "Ambient capability '" << capability << "' could not be enabled!" << std::endl;
            }
        }

        /*
         * Step 3:
         * According to the original RFC, exec follows the following rules:
         *   pA' = (file caps or setuid or setgid ? 0 : pA)
         *   pP' = (X & fP) | (pI & fI) | pA'
         *   pI' = pI
         *   pE' = (fE ? pP' : pA')
         * At this point, we sanitized pA'. Let's fix the other three sets:
         * */
        c_cap_string = cap_string.c_str();
        auto new_caps = cap_from_text(c_cap_string);
        rc = cap_set_proc(new_caps_transitional);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities, some features might not work as intended!" << std::endl;
        }
    }

    void ChildProcess::do_fork(std::map<int, int>& reg) noexcept(false) {
        if (state != READY)
            throw ChildProcessException("Process not ready, cannot fork now!");

        if (pipe(stdouterr) == -1) {
            throw ChildProcessException("Couldn't create pipe!");
        }
        fcntl(stdouterr[0], FD_CLOEXEC);

        primaryPid = fork();
        if (primaryPid == 0) {
            // This is the child that's supposed to exec
            // Note: This block will never return

            // Redirect stdout and stderr to pipe
            while (dup2(stdouterr[1], STDOUT_FILENO) == -1 && errno == EINTR) {}
            while (dup2(stdouterr[1], STDERR_FILENO) == -1 && errno == EINTR) {}
            close(stdouterr[1]);

            // Transform args
            const char* program = this->path.c_str();
            char *c_args[this->args.size()+2];
            int i = 1;
            c_args[0] = const_cast<char*>(program);
            for (auto& arg : this->args){
                auto buf = new char[arg.length()+1];
                std::strcpy(buf, arg.c_str());
                c_args[i] = buf;
                i++;
            }
            c_args[i] = nullptr;

            // Handle capabilities and drop permissions
            handle_caps();

            // Execute program
            int retval = execvp(program, c_args);

            // If we get to this point something went wrong...
            LOG->critical("Could not exec child process: {0}", retval);
            exit(-1);
        }
        close(stdouterr[1]);
        LOG->info("Child pid: {0}", primaryPid);
        state = RUNNING;
        reg[primaryPid] = graph_id;
    }

    int ChildProcess::register_with_epoll(int epoll_fd, std::map<int, int>& map) noexcept(false) {
        struct epoll_event setup{};
        setup.data.fd = stdouterr[0];
        setup.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stdouterr[0], &setup) == -1) {
            throw ChildProcessException("Couldn't bind pipe to epoll socket!");
        }
        map[stdouterr[0]] = graph_id;
    }

    std::string ChildProcess::get_name() const noexcept {
        return name;
    }

    bool ChildProcess::can_start_now() const noexcept {
        return state == READY;
    }

    void ChildProcess::notify_of_state(std::list<std::shared_ptr<ChildProcessInterface>> other_procs) noexcept {
        if (state == BLOCKED) {
            // TODO: Implement
        }
        // TODO: Implement
    }

    void ChildProcess::handle_process_event(ProcessHandlerInterface::ProcessEvent event, int data) noexcept {
        switch(event){
            case ProcessHandlerInterface::ProcessEvent::SIGHUP:
                break;
            case ProcessHandlerInterface::ProcessEvent::EXIT:
                if (state != RUNNING) {
                    LOG->warn("Child process object in state {0} notified of exit?", state);
                }
                if (data == 0)
                    state = DONE;
                else
                    state = CRASHED;
                break;
        }
    }
}