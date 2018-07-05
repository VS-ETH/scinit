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

#include "../src/ProcessHandler.h"
#include "../src/log.h"
#include "process_lifecycle_test/MockChildProcess.h"
#include "MockEventHandlers.h"

using ::testing::Invoke;
using ::testing::_;
using namespace scinit::test::lifecycle;

namespace scinit {
    class ProcessLifecycleTests : public testing::Test {
    protected:
        void SetUp() {
            if (!spdlog::get("scinit")) {
                auto console = spdlog::stdout_color_st("scinit");
                console->set_pattern("[%^%n%$] [%H:%M:%S.%e] [%l] %v");
                console->set_level(spdlog::level::critical);
            }
        }

        void TearDown() {
            spdlog::drop_all();
        }
    };

    TEST_F(ProcessLifecycleTests, SingleProcessLifecycle) {
        auto handler = std::make_shared<ProcessHandler>();

        std::list<std::string> args, capabilities;
        auto child_1 = std::make_shared<MockChildProcess>("mockproc", "/bin/false", args, "SIMPLE", capabilities,
                65534, 65534, 0, handler);
        handler->obj_for_id[0] = child_1;
        std::list<std::weak_ptr<ChildProcessInterface>> all_children;
        all_children.push_back(child_1);

        auto fork_event = new MockEventHandlers();
        EXPECT_CALL(*fork_event, call_once()).Times(1);
        EXPECT_CALL(*child_1, do_fork(_)).Times(1).WillOnce(Invoke([fork_event, &child_1](std::map<int, int>& reg) {
            fork_event->call_once();
            EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::READY);
            child_1->state = ChildProcessInterface::ProcessState::RUNNING;
            child_1->primaryPid = 0;
            reg[0] = 0;
        }));
        auto register_event = new MockEventHandlers();
        EXPECT_CALL(*register_event, call_once()).Times(1);
        EXPECT_CALL(*child_1, register_with_epoll(_, _)).Times(1).WillOnce(Invoke([register_event]
                        (int epoll_fd, std::map<int, int>& map){
            register_event->call_once();
            map[0] = 0;
        }));

        auto signal_event = new MockEventHandlers();
        EXPECT_CALL(*signal_event, call_once()).Times(1);
        handler->register_for_process_state(0, [signal_event](ProcessHandlerInterface::ProcessEvent event, int result) {
            signal_event->call_once();
            EXPECT_EQ(event, ProcessHandlerInterface::ProcessEvent::EXIT);
            EXPECT_EQ(result, 0);
        });

        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::READY);
        handler->register_processes(all_children);
        handler->start_programs();
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::RUNNING);
        EXPECT_EQ(handler->number_of_running_procs, 1);

        handler->sigchld_received(0, 0);
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::DONE);
        EXPECT_EQ(handler->number_of_running_procs, 0);

        delete signal_event;
        delete fork_event;
        delete register_event;
    }
}