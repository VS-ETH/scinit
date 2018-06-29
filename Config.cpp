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

#include <yaml-cpp/yaml.h>
#include <boost/program_options.hpp>
#include <iostream>
#include "Config.h"
#include "ConfigParseException.h"
#include "log.h"

namespace po = boost::program_options;

namespace scinit {

    Config::Config(const std::string &path) noexcept(false) {
        this->src = path;
        initialize();
    }

    void Config::initialize() noexcept(false) {
        root = YAML::LoadFile(src);

        if (!root["programs"])
            throw ConfigParseException("Config file is missing the 'programs' node!");

        LOG->info("Config loaded");
        LOG->debug("Dump\n{0}", root);
    }

    std::list<std::shared_ptr<ChildProcess>> Config::getProcesses() const noexcept {
        std::list<std::shared_ptr<ChildProcess>> results;

        YAML::Node programs = root["programs"];
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

            results.push_back(std::make_shared<ChildProcess>((*program)["name"].as<std::string>(),
                    (*program)["path"].as<std::string>(), arg_list, type, capabilities, uid, gid));
        }

        return results;
    }

    Config* handle_commandline_invocation(int argc, char** argv) noexcept(false) {
        po::options_description desc("Options");
        desc.add_options()
                ("help", "print this message")
                ("config", po::value<std::string>()->default_value("config.yml"), "path to config file")
                ("verbose", po::value<bool>()->default_value(false), "be verbose")
                ;
        po::variables_map options;
        po::store(po::parse_command_line(argc, argv, desc), options);
        po::notify(options);

        if (options.count("help")) {
            std::cout << desc << std::endl;
            exit(0);
        }

        auto console = spdlog::stdout_color_st("scinit");
        console->set_pattern("[%^%n%$] [%H:%M:%S.%e] [%l] %v");
        if (options["verbose"].as<bool>())
            console->set_level(spdlog::level::debug);
        else
            console->set_level(spdlog::level::info);

        return new scinit::Config(options["config"].as<std::string>());
    }

}