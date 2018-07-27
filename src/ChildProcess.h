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

#ifndef CINIT_CHILDPROCESS_H
#define CINIT_CHILDPROCESS_H

#include "gtest/gtest_prod.h"
#include <list>
#include <list>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>
#include "ChildProcessInterface.h"
#include "ProcessHandlerInterface.h"

namespace scinit {
    class ProcessLifecycleTests;

    // See base class for documentation
    class ChildProcess : public ChildProcessInterface {
      public:
        ChildProcess(std::string, std::string, std::list<std::string>, const std::string &, std::list<std::string>,
                     unsigned int, unsigned int, unsigned int, const std::shared_ptr<ProcessHandlerInterface> &,
                     std::list<std::string>, std::list<std::string>, bool, bool, std::list<std::string>,
                     std::list<std::pair<std::string, std::string>>);

        ChildProcess(const ChildProcess &) = delete;
        virtual ChildProcess &operator=(const ChildProcess &) = delete;
        ~ChildProcess() override = default;

        void do_fork(std::map<int, unsigned int> &) noexcept(false) override;
        void register_with_epoll(int, std::map<int, unsigned int> &,
                                 std::map<int, ProcessHandlerInterface::FDType> &) noexcept(false) override;
        std::string get_name() const noexcept override;
        unsigned int get_id() const noexcept override;
        bool can_start_now() const noexcept override;
        void notify_of_state(std::map<unsigned int, std::weak_ptr<ChildProcessInterface>>) noexcept override;
        void propagate_dependencies(std::list<std::weak_ptr<ChildProcessInterface>>) noexcept override;
        void should_wait_for(unsigned int, ProcessState) noexcept override;
        void handle_process_event(ProcessHandlerInterface::ProcessEvent event, int data) noexcept override;
        ProcessState get_state() const noexcept override;

      protected:
        std::string name, path, username;
        std::list<std::string> args, capabilities;
        unsigned int uid, gid, graph_id;
        std::shared_ptr<ProcessHandlerInterface> handler;
        std::list<std::string> before, after;
        std::list<std::pair<unsigned int, ChildProcessInterface::ProcessState>> conditions;
        int stdout[2] = {-1, -1}, stderr[2] = {-1, -1}, primaryPid = -1;
        std::vector<char> stdoutPTYName;
        std::vector<char> stderrPTYName;
        bool want_tty, want_default_env;
        ProcessType type;
        ProcessState state;

        std::unordered_set<std::string> allowed_env_vars = {"HOME", "LANG",  "LANGUAGE", "LOGNAME", "PATH",
                                                            "PWD",  "SHELL", "TERM",     "USER"};
        std::list<std::pair<std::string, std::string>> env_extra_vars;
        virtual bool handle_caps();

        FRIEND_TEST(ConfigParserTests, SmokeTestConfig);
        FRIEND_TEST(ConfigParserTests, SimpleConfDTest);
        FRIEND_TEST(ConfigParserTests, ConfigWithDeps);
        FRIEND_TEST(ConfigParserTests, ConfigWithNamedUser);
        FRIEND_TEST(ConfigParserTests, ComplexEnvConfig);
        FRIEND_TEST(ProcessLifecycleTests, SingleProcessLifecycle);
        FRIEND_TEST(ProcessLifecycleTests, TwoDependantProcessesLifecycle);
        FRIEND_TEST(IntegrationTests, TestStdOutErr);
        FRIEND_TEST(IntegrationTests, TestPty);
        FRIEND_TEST(IntegrationTests, TestPrivDrop);
        FRIEND_TEST(IntegrationTests, TestEnvFilter);
        FRIEND_TEST(IntegrationTests, TestComplexEnv);
        friend class ProcessLifecycleTests;
    };
}  // namespace scinit

#endif  // CINIT_CHILDPROCESS_H
