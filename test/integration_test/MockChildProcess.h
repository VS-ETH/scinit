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

#ifndef CINIT_MOCKCHILDPROCESS_INTEGRATION_H
#define CINIT_MOCKCHILDPROCESS_INTEGRATION_H

#include "gmock/gmock.h"
#include "../../src/ChildProcess.h"

namespace scinit {
    namespace test {
        namespace integration {
            class MockChildProcess : public ChildProcess {
              public:
                MockChildProcess(std::string name, std::string path, std::list<std::string> args,
                                 const std::string& type, std::list<std::string> capabilities, unsigned int uid,
                                 unsigned int gid, unsigned int graph_id,
                                 const std::shared_ptr<ProcessHandlerInterface>& handler, std::list<std::string> before,
                                 std::list<std::string> after, bool want_tty)
                  : ChildProcess(std::move(name), std::move(path), std::move(args), std::move(type),
                                 std::move(capabilities), uid, gid, graph_id, std::move(handler), std::move(before),
                                 std::move(after), want_tty){};

              protected:
                bool handle_caps() override { return true; };
            };
        }
    }
}  // namespace scinit

#endif  // CINIT_MOCKCHILDPROCESS_INTEGRATION_H
