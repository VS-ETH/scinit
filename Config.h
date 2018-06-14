/*
 * Copyright 2018 The cinit authors
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

#ifndef CINIT_CONFIG_H
#define CINIT_CONFIG_H

#include <string>
#include <yaml-cpp/yaml.h>
#include <memory>
#include <list>
#include "ChildProcess.h"
#include "ConfigParseException.h"

namespace cinit {
    class Config {
    public:
        Config(const std::string &path) noexcept(false);
        std::list<std::shared_ptr<ChildProcess>> getProcesses() const noexcept;

    private:
        void initialize() noexcept(false);

        std::string src;
        YAML::Node root;
    };

    Config* handle_commandline_invocation(int argc, char** argv) noexcept(false);
}


#endif //CINIT_CONFIG_H
