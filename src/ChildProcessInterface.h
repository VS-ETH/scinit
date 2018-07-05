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

#ifndef CINIT_CHILDPROCESSINTERFACE_H
#define CINIT_CHILDPROCESSINTERFACE_H

#include <string>
#include <list>
#include "ProcessHandler.h"

namespace scinit {
    class ChildProcessInterface : public std::enable_shared_from_this<ChildProcessInterface> {
    public:
        ChildProcessInterface() = default;
        // Do not copy ;)
        ChildProcessInterface(const ChildProcessInterface&) = delete;
        virtual ChildProcessInterface& operator=(const ChildProcessInterface&) = delete;

        // Fork, and register the pid to the current object
        virtual void do_fork(std::map<int, int>&) = 0;

        virtual std::string get_name() const = 0;
        virtual unsigned int get_id() const = 0;
        virtual bool can_start_now() const = 0;

        /*
         * In order to handle stdout/stderr forwarding, the pipe needs to be registered with epoll,
         * which is what this function does. It also registers the fd to the current object.
         */
        virtual void register_with_epoll(int epollfd, std::map<int, int>& fd_to_obj) = 0;

        /*
         * This function is called by the process handler with a list of all existing processes as an argument.
         * It is supposed to advance the internal state machine.
         */
        virtual void notify_of_state(std::list<std::weak_ptr<ChildProcessInterface>>) = 0;

        /*
         * Handle a process event. This function is called from the process handler and deals with process state
         * changes as indicated by signals.
         * */
        virtual void handle_process_event(ProcessHandlerInterface::ProcessEvent event, int data) = 0;

        /*
         * What kind of process is this? ONESHOT is for programs that do something and then exit and should not be
         * restarted, SIMPLE is for programs that should keep running
         * */
        enum ProcessType {
            ONESHOT,
            SIMPLE
            // TODO (maybe): FORKING
        };

        /*
         * When a new ChildProcess is created, it starts in either BLOCKED or READY state, depending whether it has
         * any dependencies that need to be fulfilled. A process moves from READY to RUNNING via an event when the
         * actual fork() takes place, when it exits, it moves (event) to DONE (type oneshot) or CRASHED (type simple).
         * At some point, CRASHED processes should move (possibly via backoff) to READY so that they can be started
         * again.
         */
        enum ProcessState {
            BLOCKED,
            READY,
            RUNNING,
            DONE,
            CRASHED,
            BACKOFF
        };
    };
}

#endif //CINIT_CHILDPROCESSINTERFACE_H

