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

cmake_minimum_required(VERSION 3.7.2)
project(cinit)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cmake)
include(CTest)
include(GoogleTest)
enable_testing()

find_package(Boost 1.62 COMPONENTS program_options filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIR})
include_directories(3rdparty/spdlog/include)

set(SCINIT_SOURCE_FILES ChildProcess.cpp ChildProcess.h Config.cpp Config.h ConfigParseException.cpp log.h
        ChildProcessException.cpp ChildProcessException.h)
# Absolute paths are a lot easier to use in test fixtures...
foreach(source ${SCINIT_SOURCE_FILES})
    list(APPEND ABS_SCINIT_SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/${source})
endforeach()

# Add GTest here, since we need 'gtest_prod.h' in src/
add_subdirectory(3rdparty/googletest gtest-build EXCLUDE_FROM_ALL)
target_include_directories(gmock_main SYSTEM BEFORE INTERFACE
        "${gtest_SOURCE_DIR}/include" "${gmock_SOURCE_DIR}/include")


# Main source
add_subdirectory(src)

# ..tests
add_subdirectory(test)
