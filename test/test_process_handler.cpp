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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "MockChildProcess.h"
#include "MockEventHandlers.h"
#include "../src/log.h"

using ::testing::Return;
using ::testing::_;

namespace scinit {
    class ProcessHandlerTests : public testing::Test {
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

    TEST_F(ProcessHandlerTests, TestOneRunnableChild) {
        auto child_1 = std::make_shared<MockChildProcess>();
        EXPECT_CALL(*child_1, can_start_now()).WillRepeatedly(Return(true));
        EXPECT_CALL(*child_1, do_fork(_)).Times(1);
        EXPECT_CALL(*child_1, register_with_epoll(_,_)).Times(1);
        EXPECT_CALL(*child_1, get_name()).Times(2).WillRepeatedly(Return("mockprog"));
        std::list<std::shared_ptr<ChildProcessInterface>> all_children;
        all_children.push_back(child_1);

        ProcessHandler handler;
        handler.register_processes(all_children);
        handler.start_programs();

        EXPECT_TRUE(spdlog::get("mockprog")) << "ProcessHandler didn't set up logger";
    }

    TEST_F(ProcessHandlerTests, TestOneChildLifecycle) {
        auto child_1 = std::make_shared<MockChildProcess>();
        EXPECT_CALL(*child_1, can_start_now()).WillRepeatedly(Return(true));
        EXPECT_CALL(*child_1, do_fork(_)).Times(1);
        EXPECT_CALL(*child_1, register_with_epoll(_,_)).Times(1);
        EXPECT_CALL(*child_1, get_name()).Times(3).WillRepeatedly(Return("mockprog"));
        std::list<std::shared_ptr<ChildProcessInterface>> all_children;
        all_children.push_back(child_1);

        ProcessHandler handler;
        handler.register_processes(all_children);
        handler.start_programs();
        EXPECT_TRUE(spdlog::get("mockprog")) << "ProcessHandler didn't set up logger";
        handler.id_for_pid[0] = 0;
        handler.obj_for_id[0] = child_1;
        EXPECT_EQ(handler.number_of_running_procs, 1);

        auto mock_events = new MockEventHandlers();
        EXPECT_CALL(*mock_events, call_once()).Times(1);
        handler.register_for_process_state(0, [mock_events](ProcessHandlerInterface::ProcessEvent event, int result) {
            mock_events->call_once();
            EXPECT_EQ(event, ProcessHandlerInterface::ProcessEvent::EXIT);
            EXPECT_EQ(result, 0);
        });

        handler.sigchld_received(0, 0);
        EXPECT_EQ(handler.number_of_running_procs, 0);
        delete mock_events;
    }
}