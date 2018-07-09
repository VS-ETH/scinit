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

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <yaml-cpp/yaml.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <numeric>
#include "ChildProcess.h"
#include "Config.h"
#include "ConfigInterface.h"
#include "ConfigParseException.h"
#include "ProcessHandler.h"
#include "log.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace scinit {
    class ProcessHandlerInterface;

    // See base class for documentation
    template <class CTYPE>
    class Config : public ConfigInterface<CTYPE> {
      public:
        Config(const std::string& path, std::shared_ptr<ProcessHandlerInterface> handler) noexcept(false)
          : handler(handler) {
            if (fs::is_directory(fs::path(path))) {
                throw ConfigParseException("This constructor is for single files only!");
            }
            load_file(path);
            LOG->info("Config loaded");
        }

        Config(const std::list<std::string>& files, std::shared_ptr<ProcessHandlerInterface> handler) noexcept(false)
          : handler(handler) {
            for (auto file : files) {
                load_file(file);
            }
            LOG->info("Config loaded");
        }

        ~Config() override = default;

        std::list<std::weak_ptr<CTYPE>> get_processes() const noexcept override {
            return std::accumulate(processes.begin(), processes.end(), std::list<std::weak_ptr<CTYPE>>(),
                                   [](auto list, auto ptr) {
                                       list.push_back(ptr);
                                       return list;
                                   });
        }

        Config(const Config&) = delete;
        virtual Config& operator=(const Config&) = delete;

      private:
        void load_file(const std::string& file) noexcept(false) {
            auto root = YAML::LoadFile(file);

            if (!root["programs"]) {
                throw ConfigParseException("Config file is missing the 'programs' node!");
            }

            LOG->debug("Dump\n{0}", root);
            parse_file(root);
        }

        std::list<std::string> yaml_node_to_str_list(YAML::iterator root, std::string node_name) {
            std::list<std::string> list;
            if ((*root)[node_name]) {
                YAML::Node node = (*root)[node_name];
                for (auto str : node) {
                    list.push_back(str.as<std::string>());
                }
            }
            return std::move(list);
        }

        void parse_file(const YAML::Node& rootNode) {
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

                std::string type = "simple";
                if ((*program)["type"]) {
                    type = (*program)["type"].as<std::string>();
                }

                std::list<std::string> arg_list = yaml_node_to_str_list(program, "args"),
                                       capabilities = yaml_node_to_str_list(program, "capabilities"),
                                       before = yaml_node_to_str_list(program, "before"),
                                       after = yaml_node_to_str_list(program, "after");

                int uid = 65534, gid = 65534;
                bool did_set_numeric = false;
                if ((*program)["uid"]) {
                    uid = (*program)["uid"].as<int>();
                    did_set_numeric = true;
                }
                if ((*program)["gid"]) {
                    uid = (*program)["gid"].as<int>();
                    did_set_numeric = true;
                }
                if ((*program)["user"]) {
                    auto user_string = (*program)["user"].as<std::string>();
                    // ...(Do not pass the returned pointer to free(3).)...
                    auto user_struct = getpwnam(user_string.c_str());
                    if (user_struct == nullptr) {
                        LOG->warn("Couldn't lookup uid of user '{0}', leaving default user!", user_string);
                    } else {
                        uid = user_struct->pw_uid;
                        if (did_set_numeric) {
                            LOG->warn("Combining explicit (user/group) with numeric (uid/gid) settings");
                        }
                    }
                }
                if ((*program)["group"]) {
                    auto group_string = (*program)["group"].as<std::string>();
                    // ...(Do not pass the returned pointer to free(3).)...
                    auto group_struct = getgrnam(group_string.c_str());
                    if (group_struct == nullptr) {
                        LOG->warn("Couldn't lookup gid of group '{0}', leaving default group!", group_string);
                    } else {
                        gid = group_struct->gr_gid;
                        if (did_set_numeric) {
                            LOG->warn("Combining explicit (user/group) with numeric (uid/gid) settings");
                        }
                    }
                }

                auto process = std::make_shared<ChildProcess>(
                  (*program)["name"].as<std::string>(), (*program)["path"].as<std::string>(), arg_list, type,
                  capabilities, uid, gid, child_counter++, handler, before, after);
                processes.push_back(process);
                handler->register_obj_id(process->get_id(), process);
            }
        }

        std::list<std::shared_ptr<CTYPE>> processes;
        int child_counter = 0;
        std::shared_ptr<ProcessHandlerInterface> handler;
    };
}  // namespace scinit

#endif  // CINIT_CONFIG_H
