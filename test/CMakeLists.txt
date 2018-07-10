#
# Copyright 2018 The scinit authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Test binaries
add_executable(zombie test_programs/zombie.cpp)
add_executable(stdouterr test_programs/stdouterr.cpp)
add_executable(ptydetect test_programs/ptydetect.cpp)

# Config unit tests

# Config parser
add_executable(config_tests ${ABS_SCINIT_SOURCE_FILES} test_config_parser.cpp)
target_link_libraries(config_tests yaml-cpp pthread util cap gmock_main ${Boost_LIBRARIES})
# Process handler
add_executable(process_handler_tests ${ABS_SCINIT_SOURCE_FILES} test_process_handler.cpp
        process_handler_test/MockChildProcess.h MockEventHandlers.h)
target_link_libraries(process_handler_tests yaml-cpp pthread util cap gmock_main ${Boost_LIBRARIES})
# Process handler and child process state machine
add_executable(process_lifecycle_tests ${ABS_SCINIT_SOURCE_FILES} test_process_lifecycle.cpp
        process_lifecycle_test/MockChildProcess.h)
target_link_libraries(process_lifecycle_tests yaml-cpp pthread util cap gmock_main ${Boost_LIBRARIES})
# Full integration test
add_executable(integration_tests ${ABS_SCINIT_SOURCE_FILES} test_integration.cpp integration_test/MockChildProcess.h integration_test/MockProcessHandler.h)
target_link_libraries(integration_tests yaml-cpp pthread util cap gmock_main ${Boost_LIBRARIES})

# Make unit tests available to 'make test'
gtest_add_tests(TARGET config_tests SOURCES test_config_parser.cpp)
gtest_add_tests(TARGET process_handler_tests SOURCES test_process_handler.cpp)
gtest_add_tests(TARGET process_lifecycle_tests SOURCES test_process_lifecycle.cpp)
gtest_add_tests(TARGET integration_tests SOURCES test_integration.cpp)