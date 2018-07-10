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

# Main executable
add_executable(scinit main.cpp ${SCINIT_SOURCE_FILES})
# Link gtest. We won't use any symbols but the dependency makes the includes available...
target_link_libraries(scinit yaml-cpp pthread util cap gtest_main ${Boost_LIBRARIES})
if (CLANG_TIDY AND CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "clang-tidy enabled for scinit")
    set_target_properties(scinit PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_ARGS}")
endif()