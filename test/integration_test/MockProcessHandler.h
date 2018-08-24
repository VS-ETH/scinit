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

#ifndef CINIT_MOCKPROCESSHANDLER_INTEGRATION_H
#define CINIT_MOCKPROCESSHANDLER_INTEGRATION_H

#include "gmock/gmock.h"
#include "../../src/ProcessHandler.h"

namespace scinit {
    namespace test {
        namespace integration {
            class MockProcessHandler : public ProcessHandler {
              public:
                bool alsoOutput = false;
                int pollRounds = -1;

              protected:
                std::string stdout, stderr;

                void handle_child_output(int fd, const std::string& str) override {
                    auto id = id_for_fd.at(fd);
                    if (auto obj = obj_for_id.at(id).lock()) {
                        std::string name = obj->get_name();
                        ASSERT_GT(fd_type.count(fd), 0);
                        switch (fd_type[fd]) {
                            case FDType::STDOUT:
                                stdout += str;
                                break;
                            case FDType::STDERR:
                                stderr += str;
                                break;
                        }
                    } else {
                        FAIL() << "Couldn't load object from list";
                    }
                    if (alsoOutput) {
                        ProcessHandler::handle_child_output(fd, str);
                    }
                }

                void start_programs() override {
                    if (pollRounds == 0) {
                        signal_received(SIGTERM);
                    }
                    if (pollRounds >= 0) {
                        pollRounds--;
                    }
                    ProcessHandler::start_programs();
                }

              public:
                const std::string getStdout() const noexcept { return stdout; }

                const std::string getStderr() const noexcept { return stderr; }
            };
        }
    }
}  // namespace scinit

#endif  // CINIT_MOCKPROCESSHANDLER_INTEGRATION_H
