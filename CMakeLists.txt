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
if (${CMAKE_VERSION} VERSION_LESS "3.9.0")
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cmake)
endif()
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
    message(STATUS "No build type specified, defaulting to Debug")
endif()
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra -Werror")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GoogleTest)
enable_testing()

find_package(Boost 1.62 COMPONENTS program_options filesystem signals REQUIRED)
include_directories(${Boost_INCLUDE_DIR})
include_directories(3rdparty/spdlog/include)

set(SCINIT_SOURCE_FILES ChildProcess.cpp ChildProcess.h Config.h ConfigParseException.cpp log.h
        ChildProcessException.cpp ChildProcessException.h ProcessHandler.cpp ProcessHandler.h
        ProcessHandlerException.cpp ProcessHandlerException.h ChildProcessInterface.h ConfigInterface.h
        ProcessHandlerInterface.h)
# Absolute paths are a lot easier to use in test targets from another subdirectory...
foreach(source ${SCINIT_SOURCE_FILES})
    list(APPEND ABS_SCINIT_SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/${source})
endforeach()
# All source files - used for clang-format
#set(ALL_SOURCE_FILES ${ABS_SCINIT_SOURCE_FILES})

# Add GTest here, since we need 'gtest_prod.h' in src/
add_subdirectory(3rdparty/googletest gtest-build EXCLUDE_FROM_ALL)
target_include_directories(gmock_main SYSTEM BEFORE INTERFACE
        "${gtest_SOURCE_DIR}/include" "${gmock_SOURCE_DIR}/include")

# Clang-tidy
# TODO(uubk): hicpp-braces-around-statements
find_program(CLANG_TIDY NAMES clang-tidy-6.0 clang-tidy-5.0 clang-tidy-3.9 clang-tidy-3.8  clang-tidy)
if(NOT CLANG_TIDY)
    message(WARNING "clang-tidy not found, checks will not be performed!")
else()
    message(STATUS "clang-tidy found: ${CLANG_TIDY}, buildtype: ${CMAKE_BUILD_TYPE}")
    set(CLANG_TIDY_ARGS "${CLANG_TIDY}")
endif()

# Inja
include_directories(AFTER SYSTEM 3rdparty/inja/src)

# Main source
add_subdirectory(src)

# Tests
add_subdirectory(test)

# Clang-format
file(GLOB_RECURSE SCINIT_TEST_SOURCE_FILES_CPP test/*.cpp)
file(GLOB_RECURSE SCINIT_TEST_SOURCE_FILES_H test/*.h)
file(GLOB_RECURSE SCINIT_MAIN_SOURCE_FILES_CPP src/*.cpp)
file(GLOB_RECURSE SCINIT_MAIN_SOURCE_FILES_H src/*.h)

set(ALL_SOURCE_FILES ${SCINIT_TEST_SOURCE_FILES_CPP} ${SCINIT_TEST_SOURCE_FILES_H} ${SCINIT_MAIN_SOURCE_FILES_CPP}
        ${SCINIT_MAIN_SOURCE_FILES_H})

add_custom_target(
        format
        COMMAND /usr/bin/clang-format
        -style=file
        -i
        ${ALL_SOURCE_FILES}
)