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

#ifndef CINIT_PROCESSHANDLERINTERFACE_H
#define CINIT_PROCESSHANDLERINTERFACE_H

#include <boost/signals2.hpp>
#include <memory>

namespace scinit {
    class ChildProcessInterface;

    class ProcessHandlerInterface {
      public:
// Somebody defines SIGHUP, so let's remove that for the enum
#define TMP_SIGHUP SIGHUP
#undef SIGHUP
        /*
         * Event notifier reasons: SIGHUP received or process exitted
         */
        enum ProcessEvent { SIGHUP, EXIT };
#define SIGHUP TMP_SIGHUP
        ProcessHandlerInterface() = default;
        virtual ~ProcessHandlerInterface() = default;
        // No copy
        ProcessHandlerInterface(const ProcessHandlerInterface&) = delete;
        virtual ProcessHandlerInterface& operator=(const ProcessHandlerInterface&) = delete;

        /*
         * Register processes. This is not in the constructor since this object has to be passed to the ChildProcesses
         * constructor.
         */
        virtual void register_processes(std::list<std::weak_ptr<ChildProcessInterface>>&) = 0;

        /*
         * Called by ChildProcesses to register for process state events
         */
        virtual void register_for_process_state(int id, std::function<void(ProcessEvent, int)> handler) = 0;

        /*
         * Called by ChildProcesses to tell us their object ID
         */
        virtual void register_obj_id(int, std::weak_ptr<ChildProcessInterface>) = 0;

        /*
         * Called by main. This function should not exit until the program is supposed to exit
         */
        virtual int enter_eventloop() = 0;

        /*
         * Type of file descriptors that are registered by child processes
         */
        enum FDType { STDOUT, STDERR };
    };
}  // namespace scinit

#endif  // CINIT_PROCESSHANDLERINTERFACE_H
