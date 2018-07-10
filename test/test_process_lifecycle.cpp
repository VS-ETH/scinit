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
#include "MockEventHandlers.h"
#include "process_lifecycle_test/MockChildProcess.h"

using ::testing::Invoke;
using ::testing::_;
using namespace scinit::test::lifecycle;

namespace scinit {
    class ProcessLifecycleTests : public testing::Test {
      protected:
        std::list<std::function<void(void)>> cleanup_functions;
        void SetUp() override {
            if (!spdlog::get("scinit")) {
                auto console = spdlog::stdout_color_st("scinit");
                console->set_pattern("[%^%n%$] [%H:%M:%S.%e] [%l] %v");
                console->set_level(spdlog::level::critical);
            }
        }

        void TearDown() override {
            spdlog::drop_all();
            for (auto fun : cleanup_functions)
                fun();
        }

        void expect_normal_run_for_child(const std::shared_ptr<MockChildProcess>& child,
                                         const std::shared_ptr<ProcessHandler>& handler, int pid) {
            MockEventHandlers *signal_event = new MockEventHandlers(), *fork_event = new MockEventHandlers(),
                              *register_event = new MockEventHandlers();
            // Child 1 mock
            EXPECT_CALL(*fork_event, call_once()).Times(1);
            EXPECT_CALL(*child, do_fork(_))
              .Times(1)
              .WillOnce(Invoke([fork_event, &child, pid](std::map<int, unsigned int>& reg) {
                  fork_event->call_once();
                  EXPECT_EQ(child->state, ChildProcessInterface::ProcessState::READY);
                  child->state = ChildProcessInterface::ProcessState::RUNNING;
                  child->primaryPid = 0;
                  reg[pid] = child->get_id();
              }));
            EXPECT_CALL(*register_event, call_once()).Times(1);
            EXPECT_CALL(*child, register_with_epoll(_, _, _))
              .Times(1)
              .WillOnce(Invoke([register_event](int, std::map<int, unsigned int>& map,
                                                std::map<int, ProcessHandlerInterface::FDType>&) {
                  register_event->call_once();
                  map[0] = 0;
              }));
            EXPECT_CALL(*signal_event, call_once()).Times(1);
            handler->register_for_process_state(
              0, [signal_event](ProcessHandlerInterface::ProcessEvent event, int result) {
                  signal_event->call_once();
                  EXPECT_EQ(event, ProcessHandlerInterface::ProcessEvent::EXIT);
                  EXPECT_EQ(result, 0);
              });
            cleanup_functions.emplace_back([signal_event, fork_event, register_event]() {
                delete signal_event;
                delete fork_event;
                delete register_event;
            });
        }
    };

    TEST_F(ProcessLifecycleTests, SingleProcessLifecycle) {
        auto handler = std::make_shared<ProcessHandler>();

        std::list<std::string> args, capabilities, before, after;
        auto child_1 = std::make_shared<MockChildProcess>("mockproc", "/bin/false", args, "SIMPLE", capabilities, 65534,
                                                          65534, 0, handler, before, after, false);
        handler->obj_for_id[0] = child_1;
        std::list<std::weak_ptr<ChildProcessInterface>> all_children;
        all_children.push_back(child_1);

        expect_normal_run_for_child(child_1, handler, 0);

        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::READY);
        handler->register_processes(all_children);
        handler->start_programs();
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::RUNNING);
        EXPECT_EQ(handler->number_of_running_procs, 1);

        handler->sigchld_received(0, 0);
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::DONE);
        EXPECT_EQ(handler->number_of_running_procs, 0);
    }

    TEST_F(ProcessLifecycleTests, TwoDependantProcessesLifecycle) {
        auto handler = std::make_shared<ProcessHandler>();

        std::list<std::string> args, capabilities, child_1_before, child_1_after, child_2_before, child_2_after;
        child_2_before.emplace_back("mockprocA");
        child_1_after.emplace_back("mockprocB");
        auto child_1 = std::make_shared<MockChildProcess>("mockprocA", "/bin/false", args, "SIMPLE", capabilities,
                                                          65534, 65534, 0, handler, child_1_before, child_1_after, false);
        auto child_2 = std::make_shared<MockChildProcess>("mockprocB", "/bin/false", args, "SIMPLE", capabilities,
                                                          65534, 65534, 1, handler, child_2_before, child_2_after, false);
        handler->obj_for_id[0] = child_1;
        handler->obj_for_id[1] = child_2;
        std::list<std::weak_ptr<ChildProcessInterface>> all_children;
        all_children.emplace_back(child_1);
        all_children.emplace_back(child_2);

        expect_normal_run_for_child(child_1, handler, 0);
        expect_normal_run_for_child(child_2, handler, 1);
        child_1->propagate_dependencies(all_children);
        child_2->propagate_dependencies(all_children);
        child_1->notify_of_state(handler->obj_for_id);
        child_2->notify_of_state(handler->obj_for_id);

        EXPECT_EQ(child_2->state, ChildProcessInterface::ProcessState::READY);
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::BLOCKED);
        handler->register_processes(all_children);
        handler->start_programs();
        EXPECT_EQ(child_2->state, ChildProcessInterface::ProcessState::RUNNING);
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::BLOCKED);
        EXPECT_EQ(handler->number_of_running_procs, 1);
        child_1->notify_of_state(handler->obj_for_id);
        child_2->notify_of_state(handler->obj_for_id);
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::READY);
        EXPECT_EQ(child_2->state, ChildProcessInterface::ProcessState::RUNNING);
        handler->start_programs();
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::RUNNING);
        EXPECT_EQ(child_2->state, ChildProcessInterface::ProcessState::RUNNING);
        EXPECT_EQ(handler->number_of_running_procs, 2);

        handler->sigchld_received(0, 0);
        handler->sigchld_received(1, 0);
        EXPECT_EQ(child_1->state, ChildProcessInterface::ProcessState::DONE);
        EXPECT_EQ(child_2->state, ChildProcessInterface::ProcessState::DONE);
        EXPECT_EQ(handler->number_of_running_procs, 0);
    }
}  // namespace scinit