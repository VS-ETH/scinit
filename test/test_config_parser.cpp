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
#include <boost/filesystem.hpp>
#include "../src/ChildProcess.h"
#include "../src/Config.h"
#include "../src/ProcessHandler.h"
#include "../src/log.h"

namespace fs = boost::filesystem;

namespace scinit {
    class ConfigParserTests : public testing::Test {
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
    };

    TEST_F(ConfigParserTests, SmokeTestConfig) {
        test_resource /= "smoke-test-config.yml";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        auto handler = std::make_shared<scinit::ProcessHandler>();
        scinit::Config<ChildProcess> uut(test_resource.native(), handler);
        auto procs = uut.get_processes();
        ASSERT_EQ(procs.size(), 2);
        for (auto &weak_proc : procs) {
            if (const auto &proc_base = weak_proc.lock().get()) {
                if (auto proc = dynamic_cast<ChildProcess *>(proc_base)) {
                    if (proc->get_name() == "ping") {
                        ASSERT_EQ(proc->path, "./ping");
                        ASSERT_EQ(proc->type, ChildProcess::ProcessType::ONESHOT);
                        ASSERT_THAT(proc->args, ::testing::ElementsAre("-c 4", "google.ch"));
                        ASSERT_THAT(proc->capabilities, ::testing::ElementsAre("CAP_NET_RAW"));
                        ASSERT_EQ(proc->uid, 65534);
                        ASSERT_EQ(proc->gid, 65534);
                    } else if (proc->get_name() == "failping") {
                        ASSERT_EQ(proc->path, "./ping");
                        ASSERT_EQ(proc->type, ChildProcess::ProcessType::ONESHOT);
                        ASSERT_THAT(proc->args, ::testing::ElementsAre("-c 4", "google.ch"));
                        ASSERT_THAT(proc->capabilities, ::testing::IsEmpty());
                        ASSERT_EQ(proc->uid, 65534);
                        ASSERT_EQ(proc->gid, 65534);
                    } else {
                        FAIL() << "Found unexpected program element";
                    }
                }
            } else {
                FAIL() << "Couldn't lock weak_ref!";
            }
        }
    }

    TEST_F(ConfigParserTests, SimpleConfDTest) {
        test_resource /= "conf.d";
        ASSERT_TRUE(fs::is_directory(test_resource)) << "Test resource missing";
        std::list<std::string> files;
        for (auto &file : fs::directory_iterator(test_resource)) {
            if (fs::is_regular(file)) {
                files.push_back(file.path().native());
            }
        }
        auto handler = std::make_shared<scinit::ProcessHandler>();
        scinit::Config<ChildProcess> uut(files, handler);
        auto procs = uut.get_processes();
        ASSERT_EQ(procs.size(), 2);
        for (auto &weak_proc : procs) {
            if (const auto &proc_base = weak_proc.lock().get()) {
                if (auto proc = dynamic_cast<ChildProcess *>(proc_base)) {
                    if (proc->get_name() == "ping") {
                        ASSERT_EQ(proc->path, "./ping");
                        ASSERT_EQ(proc->type, ChildProcess::ProcessType::ONESHOT);
                        ASSERT_THAT(proc->args, ::testing::ElementsAre("-c 4", "google.ch"));
                        ASSERT_THAT(proc->capabilities, ::testing::ElementsAre("CAP_NET_RAW"));
                        ASSERT_EQ(proc->uid, 65534);
                        ASSERT_EQ(proc->gid, 65534);
                    } else if (proc->get_name() == "failping") {
                        ASSERT_EQ(proc->path, "./ping");
                        ASSERT_EQ(proc->type, ChildProcess::ProcessType::ONESHOT);
                        ASSERT_THAT(proc->args, ::testing::ElementsAre("-c 4", "google.ch"));
                        ASSERT_THAT(proc->capabilities, ::testing::IsEmpty());
                        ASSERT_EQ(proc->uid, 65534);
                        ASSERT_EQ(proc->gid, 65534);
                    } else {
                        FAIL() << "Found unexpected program element";
                    }
                }
            } else {
                FAIL() << "Couldn't lock weak_ref!";
            }
        }
    }

    TEST_F(ConfigParserTests, ConfigWithDeps) {
        test_resource /= "config-with-deps.yml";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        auto handler = std::make_shared<scinit::ProcessHandler>();
        scinit::Config<ChildProcess> uut(test_resource.native(), handler);
        auto procs = uut.get_processes();
        ASSERT_EQ(procs.size(), 2);
        for (auto &weak_proc : procs) {
            if (const auto &proc_base = weak_proc.lock().get()) {
                if (auto proc = dynamic_cast<ChildProcess *>(proc_base)) {
                    if (proc->get_name() == "ping") {
                        ASSERT_EQ(proc->path, "./ping");
                        ASSERT_EQ(proc->type, ChildProcess::ProcessType::ONESHOT);
                        ASSERT_THAT(proc->args, ::testing::ElementsAre("-c 4", "google.ch"));
                        ASSERT_THAT(proc->capabilities, ::testing::ElementsAre("CAP_NET_RAW"));
                        ASSERT_EQ(proc->uid, 65534);
                        ASSERT_EQ(proc->gid, 65534);
                    } else if (proc->get_name() == "failping") {
                        ASSERT_EQ(proc->path, "./ping");
                        ASSERT_EQ(proc->type, ChildProcess::ProcessType::ONESHOT);
                        ASSERT_THAT(proc->args, ::testing::ElementsAre("-c 4", "google.ch"));
                        ASSERT_THAT(proc->capabilities, ::testing::IsEmpty());
                        ASSERT_EQ(proc->uid, 65534);
                        ASSERT_EQ(proc->gid, 65534);
                    } else {
                        FAIL() << "Found unexpected program element";
                    }
                }
            } else {
                FAIL() << "Couldn't lock weak_ref!";
            }
        }
    }

    TEST_F(ConfigParserTests, ConfigWithNamedUser) {
        test_resource /= "config-with-named-user.yml";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        auto handler = std::make_shared<scinit::ProcessHandler>();
        scinit::Config<ChildProcess> uut(test_resource.native(), handler);
        auto procs = uut.get_processes();
        ASSERT_EQ(procs.size(), 1);
        for (auto &weak_proc : procs) {
            if (const auto &proc_base = weak_proc.lock().get()) {
                if (auto proc = dynamic_cast<ChildProcess *>(proc_base)) {
                    if (proc->get_name() == "ping") {
                        ASSERT_EQ(proc->path, "./ping");
                        ASSERT_EQ(proc->type, ChildProcess::ProcessType::ONESHOT);
                        ASSERT_THAT(proc->args, ::testing::ElementsAre("-c 4", "google.ch"));
                        ASSERT_THAT(proc->capabilities, ::testing::ElementsAre("CAP_NET_RAW"));
                        // Nobody and nogroup should be the same on every system
                        ASSERT_EQ(proc->uid, 65534);
                        ASSERT_EQ(proc->gid, 65534);
                    } else {
                        FAIL() << "Found unexpected program element";
                    }
                }
            } else {
                FAIL() << "Couldn't lock weak_ref!";
            }
        }
    }

    TEST_F(ConfigParserTests, InvalidProgramConfig) {
        test_resource /= "invalid-program-config.yml";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        auto handler = std::make_shared<scinit::ProcessHandler>();
        scinit::Config<ChildProcess> uut(test_resource.native(), handler);
        auto procs = uut.get_processes();
        ASSERT_EQ(procs.size(), 0);
    }

    TEST_F(ConfigParserTests, BrokenYamlConfig) {
        test_resource /= "broken-yaml-config.yml";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        auto handler = std::make_shared<scinit::ProcessHandler>();
        try {
            scinit::Config<ChildProcess> uut(test_resource.native(), handler);
            FAIL() << "Expected an exception when parsing invalid YAML...";
        } catch (std::exception &e) {
            ASSERT_THAT(e.what(), ::testing::StartsWith("yaml-cpp: error at line 5, column 14: illegal EOF in scalar"));
        } catch (...) { FAIL() << "Wrong exception type"; }
    }

    TEST_F(ConfigParserTests, ComplexEnvConfig) {
        test_resource /= "test-env-complex.yml";
        ASSERT_TRUE(fs::is_regular_file(test_resource)) << "Test resource missing";
        auto handler = std::make_shared<scinit::ProcessHandler>();
        scinit::Config<ChildProcess> uut(test_resource.native(), handler);
        auto procs = uut.get_processes();
        ASSERT_EQ(procs.size(), 1);
        for (auto &weak_proc : procs) {
            if (const auto &proc_base = weak_proc.lock().get()) {
                if (auto proc = dynamic_cast<ChildProcess *>(proc_base)) {
                    if (proc->get_name() == "envtest") {
                        ASSERT_EQ(proc->path, "test/envtest");
                        // Nobody and nogroup should be the same on every system
                        ASSERT_EQ(proc->uid, 65534);
                        ASSERT_EQ(proc->gid, 65534);
                        ASSERT_EQ(proc->allowed_env_vars.size(), 10);
                        ASSERT_THAT(proc->allowed_env_vars,
                                    ::testing::UnorderedElementsAre("HOME", "LANG", "LANGUAGE", "LOGNAME", "PATH",
                                                                    "PWD", "SHELL", "TERM", "USER", "EDITOR"));
                        ASSERT_THAT(proc->env_extra_vars,
                                    ::testing::ElementsAre(std::make_pair("FOO", "bar"),
                                                           std::make_pair("BAR", "{{ USER }}-{{ FOO }}")));
                    } else {
                        FAIL() << "Found unexpected program element";
                    }
                }
            } else {
                FAIL() << "Couldn't lock weak_ref!";
            }
        }
    }
}  // namespace scinit