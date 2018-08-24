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
    class IssueReproducers : public testing::Test {
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
                console->set_level(spdlog::level::debug);
            }
        }

        void TearDown() override { spdlog::drop_all(); }
    };

    TEST_F(IssueReproducers, ReproduceIssue8) {
        test_resource /= "issue-8-simplified.yml";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        auto handler = std::make_shared<MockProcessHandler>();
        handler->alsoOutput = true;
        handler->pollRounds = 20;
        auto config = std::make_unique<scinit::Config<MockChildProcess>>(test_resource.native(), handler);
        auto child_list = config->get_processes();
        ASSERT_EQ(child_list.size(), 2);

        handler->register_processes(child_list);
        handler->enter_eventloop();

        auto stdout = handler->getStdout();
        auto doneIdx = stdout.find("done");
        auto helloIdx = stdout.find("Hello, it's me!");
        ASSERT_FALSE(doneIdx == std::string::npos || helloIdx == std::string::npos || doneIdx >= helloIdx)
          << "Invalid ordering or missing string in stdout!";
        ASSERT_EQ(handler->getStderr(), "");
    }
}