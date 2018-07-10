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

#ifndef CINIT_MOCKCHILDPROCESS_HANDLER_H
#define CINIT_MOCKCHILDPROCESS_HANDLER_H

#include "gmock/gmock.h"
#include "../../src/ChildProcessInterface.h"
#include "../../src/ProcessHandlerInterface.h"

namespace scinit {
    namespace test {
        namespace handler {
            class MockChildProcess : public ChildProcessInterface {
              public:
                MOCK_CONST_METHOD0(get_name, std::string());
                MOCK_CONST_METHOD0(get_id, unsigned int());
                MOCK_CONST_METHOD0(can_start_now, bool());
                MOCK_METHOD1(notify_of_state, void(std::map<unsigned int, std::weak_ptr<ChildProcessInterface>>));
                MOCK_METHOD1(propagate_dependencies, void(std::list<std::weak_ptr<ChildProcessInterface>>));
                MOCK_METHOD2(handle_process_event, void(ProcessHandlerInterface::ProcessEvent, int));
                MOCK_METHOD2(should_wait_for, void(unsigned int, ProcessState));
                MOCK_CONST_METHOD0(get_state, ProcessState());

                MOCK_METHOD1(do_fork, void(std::map<int, unsigned int> &));
                MOCK_METHOD3(register_with_epoll, void(int, std::map<int, unsigned int> &,
                                                       std::map<int, ProcessHandlerInterface::FDType> &));
            };
        }
    }
}  // namespace scinit

#endif  // CINIT_MOCKCHILDPROCESS_HANDLER_H
