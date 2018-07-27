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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "../src/Config.h"
#include "../src/log.h"
#include "MockEventHandlers.h"
#include "integration_test/MockChildProcess.h"
#include "integration_test/MockProcessHandler.h"

using ::testing::Return;
using ::testing::_;
using namespace scinit::test::integration;

namespace scinit {
    class IntegrationTests : public testing::Test {
      protected:
        fs::path test_resource;

        void SetUp() override {
            test_resource = fs::path(__FILE__);
            ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Path to source does not point to a regular file";
            test_resource.remove_filename();
            test_resource /= "test_configs";
            ASSERT_TRUE(fs::is_directory(test_resource)) << "Test resource directory is missing";
            if (!spdlog::get("scinit")) {
                auto console = spdlog::stdout_color_st("scinit");
                console->set_pattern("[%^%n%$] [%H:%M:%S.%e] [%l] %v");
                console->set_level(spdlog::level::critical);
            }
        }

        void TearDown() override { spdlog::drop_all(); }
    };

    TEST_F(IntegrationTests, TestStdOutErr) {
        auto test_program = fs::path(test_resource);
        test_resource /= "test-stdouterr.yml";
        test_program = test_program.parent_path();
        test_program = test_program.parent_path();
        test_program /= "build";
        test_program /= "test";
        test_program /= "stdouterr";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        ASSERT_TRUE(fs::is_regular_file(test_program)) << "Test resource missing";
        auto handler = std::make_shared<MockProcessHandler>();
        auto config = std::make_unique<scinit::Config<MockChildProcess>>(test_resource.native(), handler);
        auto child_list = config->get_processes();
        ASSERT_EQ(child_list.size(), 1);
        auto child_ptr = dynamic_cast<MockChildProcess *>(child_list.begin()->lock().get());
        child_ptr->path = test_program.native();

        handler->register_processes(child_list);
        handler->should_quit = true;
        ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::READY);
        handler->enter_eventloop();
        ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::DONE);
        ASSERT_EQ(handler->getStdout(), "Something to stdout");
        ASSERT_EQ(handler->getStderr(), "Something to stderr");
    }

    TEST_F(IntegrationTests, TestPty) {
        auto test_program = fs::path(test_resource);
        test_resource /= "test-pty.yml";
        test_program = test_program.parent_path();
        test_program = test_program.parent_path();
        test_program /= "build";
        test_program /= "test";
        test_program /= "ptydetect";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        ASSERT_TRUE(fs::is_regular_file(test_program)) << "Test resource missing";
        auto handler = std::make_shared<MockProcessHandler>();
        auto config = std::make_unique<scinit::Config<MockChildProcess>>(test_resource.native(), handler);
        auto child_list = config->get_processes();
        ASSERT_EQ(child_list.size(), 1);
        auto child_ptr = dynamic_cast<MockChildProcess *>(child_list.begin()->lock().get());
        child_ptr->path = test_program.native();

        handler->register_processes(child_list);
        handler->should_quit = true;
        ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::READY);
        handler->enter_eventloop();
        ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::DONE);
        ASSERT_EQ(handler->getStdout(), "stdout is a tty");
        ASSERT_EQ(handler->getStderr(), "stderr is a tty");
    }

    TEST_F(IntegrationTests, TestPrivDrop) {
        if (geteuid() != 0) {
            // Without root, this doesn't make any sense
            // GTest can't `skip` test at runtime at the moment until PR 1544 has landed
            std::cerr << "Warning: Skipping 'TestPrivDrop' as this test requires root" << std::endl;
        } else {
            auto test_program = fs::path(test_resource);
            test_resource /= "test-pty.yml";
            test_program = test_program.parent_path();
            test_program = test_program.parent_path();
            test_program /= "build";
            test_program /= "test";
            test_program /= "permdetect";
            ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
            ASSERT_TRUE(fs::is_regular_file(test_program)) << "Test resource missing";
            auto handler = std::make_shared<MockProcessHandler>();
            auto config = std::make_unique<scinit::Config<ChildProcess>>(test_resource.native(), handler);
            auto child_list = config->get_processes();
            ASSERT_EQ(child_list.size(), 1);
            auto child_ptr = dynamic_cast<ChildProcess *>(child_list.begin()->lock().get());
            child_ptr->path = test_program.native();

            handler->register_processes(child_list);
            handler->should_quit = true;
            ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::READY);
            handler->enter_eventloop();
            ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::DONE)
              << "Full stderr:" << handler->getStderr() << ", Full stdout: " << handler->getStdout();
            ASSERT_EQ(handler->getStderr(), "") << "Full stderr:" << handler->getStderr()
                                                << ", Full stdout: " << handler->getStdout();
            ASSERT_EQ(handler->getStdout(), "Test via FD\nTest via stream")
              << "Full stderr:" << handler->getStderr() << ", Full stdout: " << handler->getStdout();
        }
    }

    TEST_F(IntegrationTests, TestEnvFilter) {
        auto test_program = fs::path(test_resource);
        test_resource /= "test-env.yml";
        test_program = test_program.parent_path();
        test_program = test_program.parent_path();
        test_program /= "build";
        test_program /= "test";
        test_program /= "envdetect";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        ASSERT_TRUE(fs::is_regular_file(test_program)) << "Test resource missing";
        auto handler = std::make_shared<MockProcessHandler>();
        auto config = std::make_unique<scinit::Config<MockChildProcess>>(test_resource.native(), handler);
        auto child_list = config->get_processes();
        ASSERT_EQ(child_list.size(), 1);
        auto child_ptr = dynamic_cast<MockChildProcess *>(child_list.begin()->lock().get());
        child_ptr->path = test_program.native();

        handler->register_processes(child_list);
        handler->should_quit = true;
        ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::READY);
        handler->enter_eventloop();
        ASSERT_EQ(handler->getStderr(), "");
        ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::DONE);
    }

    TEST_F(IntegrationTests, TestComplexEnv) {
        auto test_program = fs::path(test_resource);
        test_resource /= "test-env-complex-it.yml";
        test_program = test_program.parent_path();
        test_program = test_program.parent_path();
        test_program /= "build";
        test_program /= "test";
        test_program /= "envcompare";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        ASSERT_TRUE(fs::is_regular_file(test_program)) << "Test resource missing";
        auto handler = std::make_shared<MockProcessHandler>();
        auto config = std::make_unique<scinit::Config<MockChildProcess>>(test_resource.native(), handler);
        auto child_list = config->get_processes();
        ASSERT_EQ(child_list.size(), 1);
        auto child_ptr = dynamic_cast<MockChildProcess *>(child_list.begin()->lock().get());
        child_ptr->path = test_program.native();

        handler->register_processes(child_list);
        handler->should_quit = true;
        ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::READY);
        handler->enter_eventloop();
        ASSERT_EQ(handler->getStderr(), "");
        ASSERT_EQ(child_ptr->get_state(), ChildProcessInterface::ProcessState::DONE);
    }
}  // namespace scinit