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

# Config unit tests
add_executable(config_tests ${ABS_SCINIT_SOURCE_FILES} test_config_parser.cpp)
target_link_libraries(config_tests yaml-cpp pthread cap gmock_main ${Boost_LIBRARIES})
# Make unit tests available to 'make test'
gtest_add_tests(TARGET config_tests SOURCES test_config_parser.cpp)