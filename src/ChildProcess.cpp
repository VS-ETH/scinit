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

#include "ChildProcess.h"
#include <fcntl.h>
#include <sys/capability.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include "ChildProcessException.h"
#include "log.h"

namespace scinit {
    ChildProcess::ChildProcess(std::string name, std::string path, std::list<std::string> args, const std::string& type,
                               std::list<std::string> capabilities, unsigned int uid, unsigned int gid,
                               unsigned int graph_id, const std::shared_ptr<ProcessHandlerInterface>& handler,
                               std::list<std::string> before, std::list<std::string> after)
      : name(std::move(name)), path(std::move(path)), args(std::move(args)), capabilities(std::move(capabilities)),
        uid(uid), gid(gid), graph_id(graph_id), handler(handler), before(std::move(before)), after(std::move(after)) {
        if (before.empty() && after.empty())
            this->state = READY;
        else
            this->state = BLOCKED;

        if (type == "simple") {
            this->type = SIMPLE;
        } else {
            this->type = ONESHOT;
        }

        auto ref = std::bind(&ChildProcess::handle_process_event, this, std::placeholders::_1, std::placeholders::_2);
        handler->register_for_process_state(graph_id, ref);
    }

    unsigned int ChildProcess::get_id() const noexcept { return graph_id; }

    void ChildProcess::handle_caps() {
        // Step 1: Make sure that the this process has all caps that we need
        std::string cap_string = "CAP_SETUID=eip CAP_SETGID=eip CAP_SETPCAP=eip";
        cap_t new_caps_transitional;
        for (const auto& capability : capabilities)
            cap_string += " " + capability + "=eip";
        const char* c_cap_string = cap_string.c_str();
        new_caps_transitional = cap_from_text(c_cap_string);

        int rc = cap_set_proc(new_caps_transitional);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities, some features might not work as intended!" << std::endl;
        }

        // Keep caps across setuid because otherwise we can't set ambient caps below
        // NOLINTNEXTLINE(hicpp-vararg)
        prctl(PR_SET_KEEPCAPS, 1);
        // Set user and group
        if (setgid(this->gid) == -1)
            std::cerr << "Couldn't set group!" << std::endl;
        if (setuid(this->uid) == -1)
            std::cerr << "Couldn't set user!" << std::endl;
        // NOLINTNEXTLINE(hicpp-vararg)
        prctl(PR_SET_KEEPCAPS, 0);

        rc = cap_set_proc(new_caps_transitional);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities, some features might not work as intended!" << std::endl;
        }

        // Step 2: Make sure that the child process only retains the capabilities explicitly specified
        // Clear all ambient capabielities and
        // NOLINTNEXTLINE(hicpp-vararg)
        prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
        // ... re-add those we need explicitly
        cap_string = "";
        for (const auto& capability : capabilities) {
            cap_string += " " + capability + "=eip";
            cap_value_t cap = {};
            if (cap_from_name(capability.c_str(), &cap) != 0) {
                std::cerr << "Warning: Couldn't decode '" << capability << "', not adding to set." << std::endl;
            }
            // NOLINTNEXTLINE(hicpp-vararg)
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
        rc = cap_set_proc(new_caps);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities, some features might not work as intended!" << std::endl;
        }
    }

    void ChildProcess::do_fork(std::map<int, unsigned int>& reg) noexcept(false) {
        if (state != READY)
            throw ChildProcessException("Process not ready, cannot fork now!");

        if (pipe(static_cast<int*>(stdouterr)) == -1) {
            throw ChildProcessException("Couldn't create pipe!");
        }
        // NOLINTNEXTLINE(hicpp-vararg)
        fcntl(stdouterr[0], FD_CLOEXEC);

        primaryPid = fork();
        if (primaryPid == 0) {
            // This is the child that's supposed to exec
            // Note: This block will never return

            // Redirect stdout and stderr to pipe
            while (dup2(stdouterr[1], STDOUT_FILENO) == -1 && errno == EINTR) {
            }
            while (dup2(stdouterr[1], STDERR_FILENO) == -1 && errno == EINTR) {
            }
            close(stdouterr[1]);

            // Transform args
            const char* program = this->path.c_str();
            auto c_args = new char*[this->args.size() + 2];
            int i = 1;
            c_args[0] = const_cast<char*>(program);
            for (auto& arg : this->args) {
                auto buf = new char[arg.length() + 1];
                std::strncpy(buf, arg.c_str(), arg.length());
                c_args[i] = buf;
                i++;
            }
            c_args[i] = nullptr;

            // Handle capabilities and drop permissions
            handle_caps();

            // Execute program
            int retval = execvp(program, static_cast<char* const*>(c_args));

            // If we get to this point something went wrong...
            LOG->critical("Could not exec child process: {0}", retval);
            exit(-1);
        }
        close(stdouterr[1]);
        LOG->info("Child pid: {0}", primaryPid);
        state = RUNNING;
        reg[primaryPid] = graph_id;
    }

    void ChildProcess::register_with_epoll(int epoll_fd, std::map<int, unsigned int>& map) noexcept(false) {
        struct epoll_event setup {};
        setup.data.fd = stdouterr[0];
        setup.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stdouterr[0], &setup) == -1) {
            throw ChildProcessException("Couldn't bind pipe to epoll socket!");
        }
        map[stdouterr[0]] = graph_id;
    }

    std::string ChildProcess::get_name() const noexcept { return name; }

    bool ChildProcess::can_start_now() const noexcept { return state == READY; }

    void ChildProcess::should_wait_for(unsigned int other_process, ProcessState other_state) noexcept {
        bool contains =
          std::accumulate(conditions.begin(), conditions.end(), false, [other_process](bool contains, auto pair) {
              if (contains)
                  return true;
              if (pair.first == other_process)
                  return true;
              return false;
          });
        if (!contains)
            conditions.emplace_back(std::make_pair(other_process, other_state));
    }

    void ChildProcess::propagate_dependencies(
      std::list<std::weak_ptr<scinit::ChildProcessInterface>> other_processes) noexcept {
        auto func = [&other_processes, classref = this](auto dependency, bool other_or_this) {
            for (const auto& weak_ref : other_processes) {
                if (auto ref = weak_ref.lock()) {
                    if (ref->get_name() == dependency) {
                        if (other_or_this)
                            ref->should_wait_for(classref->graph_id,
                                                 classref->type == SIMPLE ? ProcessState::DONE : ProcessState::RUNNING);
                        else
                            classref->should_wait_for(
                              ref->get_id(), classref->type == SIMPLE ? ProcessState::DONE : ProcessState::RUNNING);
                    }
                }
            }
        };
        std::for_each(before.begin(), before.end(), [&func, this](auto arg) { func(arg, true); });
        std::for_each(after.begin(), after.end(), [&func, this](auto arg) { func(arg, false); });
        before.clear();
        after.clear();
    }

    ChildProcessInterface::ProcessState ChildProcess::get_state() const noexcept { return state; }

    void ChildProcess::notify_of_state(
      std::map<unsigned int, std::weak_ptr<ChildProcessInterface>> other_procs) noexcept {
        if (state == BLOCKED) {
            bool still_blocked = std::accumulate(
              conditions.begin(), conditions.end(), false, [&other_procs, this](bool blocked, auto condition) {
                  if (blocked)
                      return true;
                  if (!other_procs.count(condition.first)) {
                      LOG->critical("BUG: Found reference to process that doesn't exist");
                      return true;
                  }
                  if (auto ptr = other_procs[condition.first].lock()) {
                      return ptr->get_state() != condition.second;
                  }
                  LOG->critical("BUG: Found reference to process that doesn't exist anymore");
                  return true;
              });
            if (!still_blocked)
                state = READY;
        }
    }

    void ChildProcess::handle_process_event(ProcessHandlerInterface::ProcessEvent event, int data) noexcept {
        switch (event) {
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
}  // namespace scinit