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

#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <csignal>
#include <iostream>
#include <mutex>
#include "ChildProcess.h"
#include "ChildProcessException.h"
#include "ChildProcessInterface.h"
#include "Config.h"
#include "ProcessHandler.h"
#include "log.h"

#define MAX_EVENTS 10
#define BUF_SIZE 4096

scinit::Config<scinit::ChildProcessInterface>* handle_commandline_invocation(
  int argc, char** argv, const std::shared_ptr<scinit::ProcessHandlerInterface>& handler) noexcept(false) {
    po::options_description desc("Options");
    desc.add_options()("help", "print this message")("config", po::value<std::string>()->default_value("config.yml"),
                                                     "path to config file")(
      "verbose", po::value<bool>()->default_value(false), "be verbose");
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

    auto config = options["config"].as<std::string>();
    // Check whether 'config' is a file or a directory
    fs::path config_path(config);
    if (!fs::exists(config_path)) {
        LOG->critical("Config file '{0}' does not exist, aborting!", config);
        return nullptr;
    }
    if (fs::is_directory(config_path)) {
        std::list<std::string> files;
        for (auto& file : fs::directory_iterator(config_path)) {
            if (fs::is_regular(file))
                files.push_back(file.path().native());
        }
        return new scinit::Config<scinit::ChildProcessInterface>(files, handler);
    }
    return new scinit::Config<scinit::ChildProcessInterface>(config, handler);
}

int main(int argc, char** argv) {
    auto handler = std::make_shared<scinit::ProcessHandler>();

    auto conf = handle_commandline_invocation(argc, argv, handler);
    if (conf == nullptr)
        return -1;
    auto child_list = conf->get_processes();
    handler->register_processes(child_list);

    return handler->enter_eventloop();
}