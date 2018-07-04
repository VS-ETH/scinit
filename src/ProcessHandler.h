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

#ifndef CINIT_PROCESSHANDLER_H
#define CINIT_PROCESSHANDLER_H

#include "ProcessHandlerInterface.h"
#include "gtest/gtest_prod.h"

namespace scinit {
    // See base class for documentation
    class ProcessHandler : public ProcessHandlerInterface {
    public:
        ProcessHandler() = default;
        ProcessHandler(const ProcessHandler&) = delete;
        virtual ProcessHandler& operator=(const ProcessHandler&) = delete;

        void register_processes(std::list<std::shared_ptr<ChildProcessInterface>>&) override;
        void register_for_process_state(int id, std::function<void(ProcessHandlerInterface::ProcessEvent, int)> handler)
                                            override;
        void register_obj_id(int, std::shared_ptr<ChildProcessInterface>) override;
        int enter_eventloop() override;

    private:
        std::map<int, boost::signals2::signal<void(ProcessHandlerInterface::ProcessEvent, int)>*> sig_for_id;
        std::map<int, std::shared_ptr<ChildProcessInterface>> obj_for_id;
        std::map<int, int> id_for_pid;
        std::map<int, int> id_for_fd;
        std::list<std::shared_ptr<ChildProcessInterface>> all_objs;
        int epoll_fd = -1, signal_fd = -1 , number_of_running_procs = 0;
        bool should_quit = false;

        void setup_signal_handlers();
        void start_programs();
        void event_received(int fd, unsigned int event);
        void signal_received(int signal);
        void sigchld_received(int pid, int rc);

        FRIEND_TEST(ProcessHandlerTests, TestOneRunnableChild);
        FRIEND_TEST(ProcessHandlerTests, TestOneChildLifecycle);
    };
}

#endif //CINIT_PROCESSHANDLER_H
