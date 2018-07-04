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

#ifndef CINIT_CONFIG_H
#define CINIT_CONFIG_H

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include "ConfigInterface.h"
#include "Config.h"
#include "ConfigParseException.h"
#include "ChildProcess.h"
#include "ProcessHandler.h"
#include "log.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace scinit {
    class ProcessHandlerInterface;

    // See base class for documentation
    template <class CTYPE> class Config : public ConfigInterface<CTYPE> {
    public:
        Config(const std::string &path, std::shared_ptr<ProcessHandlerInterface> handler) noexcept(false) :
                handler(handler) {
            if (fs::is_directory(fs::path(path)))
                throw ConfigParseException("This constructor is for single files only!");
            loadFile(path);
            LOG->info("Config loaded");
        }

        Config(const std::list<std::string> &files, std::shared_ptr<ProcessHandlerInterface> handler) noexcept(false) :
                handler(handler) {
            for (auto file : files) {
                loadFile(file);
            }
            LOG->info("Config loaded");
        }

        std::list<std::shared_ptr<CTYPE>> get_processes() const noexcept override {
            return processes;
        }

        Config(const Config&) = delete;
        virtual Config& operator=(const Config&) = delete;

    private:
        void loadFile(const std::string& file) noexcept(false) {
            auto root = YAML::LoadFile(file);

            if (!root["programs"])
                throw ConfigParseException("Config file is missing the 'programs' node!");

            LOG->debug("Dump\n{0}", root);
            parseFile(root);
        }

        void parseFile(const YAML::Node & rootNode) {
            YAML::Node programs = rootNode["programs"];
            for (auto program = programs.begin(); program != programs.end(); program++) {
                if (!(*program)["name"]) {
                    LOG->warn("Program entry has no name, skipping!");
                    continue;
                }

                if (!(*program)["path"]) {
                    LOG->warn("Program '{0}' has no executable path, skipping!", (*program)["name"]);
                    continue;
                }

                std::list<std::string> arg_list;
                if ((*program)["args"]) {
                    YAML::Node args = (*program)["args"];
                    for (auto arg : args)
                        arg_list.push_back(arg.as<std::string>());
                }

                std::string type = "simple";
                if ((*program)["type"]) {
                    type = (*program)["type"].as<std::string>();
                }

                std::list<std::string> capabilities;
                if ((*program)["capabilities"]) {
                    YAML::Node caps = (*program)["capabilities"];
                    for (auto capability : caps)
                        capabilities.push_back(capability.as<std::string>());
                }

                int uid = 65534, gid = 65534;
                if ((*program)["uid"]) {
                    uid = (*program)["uid"].as<int>();
                }
                if ((*program)["gid"]) {
                    uid = (*program)["gid"].as<int>();
                }

                auto process = std::make_shared<ChildProcess>(
                        (*program)["name"].as<std::string>(),(*program)["path"].as<std::string>(), arg_list, type,
                        capabilities, uid, gid, child_counter++, handler);
                processes.push_back(process);
                handler->register_obj_id(process->get_id(), process);
            }
        }

        std::list<std::shared_ptr<CTYPE>> processes;
        int child_counter = 0;
        std::shared_ptr<ProcessHandlerInterface> handler;
    };
}

#endif //CINIT_CONFIG_H
