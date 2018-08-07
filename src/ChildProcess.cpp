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

#include "ChildProcess.h"
#include <fcntl.h>
#include <pty.h>
#include <pwd.h>
#include <sys/capability.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include "ChildProcessException.h"
#include "inja.hpp"
#include "log.h"

namespace scinit {
    ChildProcess::ChildProcess(std::string name, std::string path, std::list<std::string> args, const std::string& type,
                               std::list<std::string> capabilities, unsigned int uid, unsigned int gid,
                               unsigned int graph_id, const std::shared_ptr<ProcessHandlerInterface>& handler,
                               std::list<std::string> before, std::list<std::string> after, bool want_tty,
                               bool want_default_env, std::list<std::string> env_extra_whitelist,
                               std::list<std::pair<std::string, std::string>> env_extra_vars)
      : name(std::move(name)), path(std::move(path)), args(std::move(args)), capabilities(std::move(capabilities)),
        uid(uid), gid(gid), graph_id(graph_id), handler(handler), before(std::move(before)), after(std::move(after)),
        want_tty(want_tty), want_default_env(want_default_env), env_extra_vars(std::move(env_extra_vars)) {
        if (this->before.empty() && this->after.empty()) {
            this->state = READY;
        } else {
            this->state = BLOCKED;
        }

        if (type == "simple") {
            this->type = SIMPLE;
        } else {
            this->type = ONESHOT;
        }

        auto ref = std::bind(&ChildProcess::handle_process_event, this, std::placeholders::_1, std::placeholders::_2);
        handler->register_for_process_state(graph_id, ref);
        auto user_ref = getpwuid(uid);  // Must not be free'd
        if (user_ref == nullptr) {
            username = "UNKNOWN";
        } else {
            username = std::string(user_ref->pw_name);
        }

        for (const auto& element : env_extra_whitelist) {
            allowed_env_vars.insert(element);
        }
    }

    unsigned int ChildProcess::get_id() const noexcept { return graph_id; }

    bool ChildProcess::handle_caps() {
        // Change owner of tty if necessary. Do this before touching capabilities!
        if (want_tty) {
            if (chown(stdoutPTYName.data(), this->uid, this->gid) != 0) {
                std::cerr << "Couldn't set owner of stdout PTY!" << std::endl;
                return false;
            }
            if (chown(stderrPTYName.data(), this->uid, this->gid) != 0) {
                std::cerr << "Couldn't set owner of stderr PTY!" << std::endl;
                return false;
            }
            if (chmod(stdoutPTYName.data(), 0620) != 0) {
                std::cerr << "Couldn't set mode of stdout PTY!" << std::endl;
                return false;
            }
            if (chmod(stderrPTYName.data(), 0620) != 0) {
                std::cerr << "Couldn't set mode of stderr PTY!" << std::endl;
                return false;
            }
        }

        // Step 1: Make sure that the this process has all caps that we need
        std::string cap_string = "CAP_SETUID=eip CAP_SETGID=eip CAP_SETPCAP=eip";
        cap_t new_caps_transitional;
        for (const auto& capability : capabilities) {
            cap_string += " " + capability + "=eip";
        }
        const char* c_cap_string = cap_string.c_str();
        new_caps_transitional = cap_from_text(c_cap_string);

        int rc = cap_set_proc(new_caps_transitional);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities!" << std::endl;
            return false;
        }

        // Keep caps across setuid because otherwise we can't set ambient caps below
        // NOLINTNEXTLINE(hicpp-vararg)
        prctl(PR_SET_KEEPCAPS, 1);
        // Set user and group
        if (setgid(this->gid) == -1) {
            std::cerr << "Couldn't set group!" << std::endl;
            return false;
        }
        if (setuid(this->uid) == -1) {
            std::cerr << "Couldn't set user!" << std::endl;
            return false;
        }
        // NOLINTNEXTLINE(hicpp-vararg)
        prctl(PR_SET_KEEPCAPS, 0);

        rc = cap_set_proc(new_caps_transitional);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities!" << std::endl;
            return false;
        }
        cap_free(new_caps_transitional);

        // Step 2: Make sure that the child process only retains the capabilities explicitly specified
        // Clear all ambient capabielities and
        // NOLINTNEXTLINE(hicpp-vararg)
        prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
        // ... re-add those we need explicitly
        cap_string = "";
        for (const auto& capability : capabilities) {
            cap_string += " " + capability + "=eip";
            cap_value_t cap = {};
            if (cap_from_name(capability.c_str(), &cap) != 0) {
                std::cerr << "Warning: Couldn't decode '" << capability << "', not adding to set." << std::endl;
            }
            // NOLINTNEXTLINE(hicpp-vararg)
            if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) != -1) {
                std::cout << "Ambient capability '" << capability << "' enabled." << std::endl;
            } else {
                std::cout << "Ambient capability '" << capability << "' could not be enabled!" << std::endl;
                return false;
            }
        }

        /*
         * Step 3:
         * According to the original RFC, exec follows the following rules:
         *   pA' = (file caps or setuid or setgid ? 0 : pA)
         *   pP' = (X & fP) | (pI & fI) | pA'
         *   pI' = pI
         *   pE' = (fE ? pP' : pA')
         * At this point, we sanitized pA'. Let's fix the other three sets:
         * */
        c_cap_string = cap_string.c_str();
        auto new_caps = cap_from_text(c_cap_string);
        rc = cap_set_proc(new_caps);
        if (rc != 0) {
            std::cerr << "Couldn't set capabilities!" << std::endl;
            return false;
        }
        cap_free(new_caps);
        return true;
    }

    std::list<std::string> ChildProcess::handle_env() {
        nlohmann::json envObj;
        auto injaEnv = inja::Environment();
        injaEnv.add_callback("keys", 1, [&injaEnv](inja::Parsed::Arguments args, nlohmann::json data) {
            nlohmann::json obj = injaEnv.get_argument(args, 0, data);
            nlohmann::json list;
            for (nlohmann::json::iterator it = obj.begin(); it != obj.end(); ++it) {
                list.emplace_back(it.key());
            }
            return list;
        });
        injaEnv.add_callback("lookup", 2, [&injaEnv](inja::Parsed::Arguments args, nlohmann::json data) {
            nlohmann::json obj = injaEnv.get_argument(args, 0, data);
            auto key = injaEnv.get_argument<std::string>(args, 1, data);
            return obj[key];
        });

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        for (int i = 0; environ[i] != nullptr; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            std::string var(environ[i]);
            auto pos = var.find('=');
            if (pos <= 0) {
                throw ChildProcessException("ERROR: Couldn't find '=' in environment variable string!");
            }
            auto key = var.substr(0, pos);
            if (allowed_env_vars.count(key) == 1) {
                auto val = var.substr(pos + 1);
                envObj["vars"][key] = val;
            }
        }
        // Force USER to correct value
        envObj["vars"]["USER"] = username;
        if (want_default_env) {
            envObj["vars"]["LANG"] = "C";
            envObj["vars"]["LANGUAGE"] = "en";
            envObj["vars"]["LOGNAME"] = username;
            envObj["vars"]["PATH"] = "/usr/local/bin:/usr/bin:/bin";
            envObj["vars"]["SHELL"] = "/bin/bash";
            envObj["vars"]["TERM"] = "screen";
            envObj["vars"]["HOME"] = "/app";
            envObj["vars"]["PWD"] = "/app";
        }
        for (const auto& pair : env_extra_vars) {
            auto val = inja::render(pair.second, envObj);
            auto str = pair.first + "=" + val;
            envObj["vars"][pair.first] = val;
        }
        auto envstr =
          injaEnv.render("{% for var in keys(vars) %}{{ var }}={{ lookup(vars, var) }}\n{% endfor %}", envObj);
        std::stringstream stream(envstr);
        std::string line;
        std::list<std::string> environment;
        while (std::getline(stream, line, '\n')) {
            environment.push_back(line);
        }
        return environment;
    }

    void ChildProcess::do_fork(std::map<int, unsigned int>& reg) noexcept(false) {
        if (state != READY) {
            throw ChildProcessException("Process not ready, cannot fork now!");
        }

        if (!want_tty) {
            if (pipe(static_cast<int*>(stdout)) == -1) {
                throw ChildProcessException("Couldn't create stdout pipe!");
            }
            if (pipe(static_cast<int*>(stderr)) == -1) {
                throw ChildProcessException("Couldn't create stderr pipe!");
            }
            // NOLINTNEXTLINE(hicpp-vararg)
            fcntl(stdout[0], FD_CLOEXEC);
            // NOLINTNEXTLINE(hicpp-vararg)
            fcntl(stderr[0], FD_CLOEXEC);
        } else {
            // User wants a PTY. PTYs mimic real terminals. Let's see if this process has a terminal so that we can
            // duplicate it's properties
            struct winsize term_size {};
            struct termios term_properties {};

            // NOLINTNEXTLINE(hicpp-vararg)
            if (tcgetattr(STDIN_FILENO, &term_properties) < 0 || ioctl(STDIN_FILENO, TIOCGWINSZ, &term_size) < 0) {
                LOG->warn("PTY requested, but we're not attached to a TTY ourselves. Faking TTY properties...");
                term_size.ws_xpixel = 0;
                term_size.ws_ypixel = 0;
                term_size.ws_row = 24;
                term_size.ws_col = 80;
                // TODO(uubk): term_properties
            }
            // NOLINTNEXTLINE(hicpp-signed-bitwise)
            term_properties.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            // NOLINTNEXTLINE(hicpp-signed-bitwise)
            term_properties.c_oflag &= ~(OPOST);
            // NOLINTNEXTLINE(hicpp-signed-bitwise)
            term_properties.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

            if (openpty(&stdout[0], &stdout[1], nullptr, &term_properties, &term_size) < 0) {
                throw ChildProcessException("Couldn't allocate stdout pty!");
            }
            if (openpty(&stderr[0], &stderr[1], nullptr, &term_properties, &term_size) < 0) {
                throw ChildProcessException("Couldn't allocate stderr pty!");
            }
            int rc;
            stdoutPTYName.reserve(64);
            stderrPTYName.reserve(64);
            while ((rc = ttyname_r(stdout[1], stdoutPTYName.data(), stdoutPTYName.capacity())) == ERANGE) {
                stdoutPTYName.reserve(stdoutPTYName.capacity() * 2 + 1);
            }
            if (rc != 0) {
                throw ChildProcessException("Couldn't load name of stdout pty!");
            }
            while ((rc = ttyname_r(stdout[1], stderrPTYName.data(), stderrPTYName.capacity())) == ERANGE) {
                stderrPTYName.reserve(stderrPTYName.capacity() * 2 + 1);
            }
            if (rc != 0) {
                throw ChildProcessException("Couldn't load name of stderr pty!");
            }
        }

        auto environment = handle_env();

        primaryPid = fork();
        if (primaryPid == 0) {
            /* This is the child that's supposed to exec
             *
             * Note: This block will never return
             *
             * Since it will never return the destructors of the strings (arguments and name)
             * will never be called, nor will the strings be used again. It is therefore acceptable
             * to remove the const from c_str() and pass them directly to execvp
             */

            // Redirect stdout and stderr to pipe
            while (dup2(stdout[1], STDOUT_FILENO) == -1 && errno == EINTR) {
            }
            while (dup2(stderr[1], STDERR_FILENO) == -1 && errno == EINTR) {
            }
            close(stdout[1]);
            close(stderr[1]);
            // Transform args
            const char* program = this->path.c_str();
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            auto c_args = new char*[this->args.size() + 2];
            int i = 1;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
            c_args[0] = const_cast<char*>(program);
            for (auto& arg : this->args) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
                c_args[i] = const_cast<char*>(arg.c_str());
                i++;
            }
            c_args[i] = nullptr;

            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            auto c_env = new char*[environment.size() + 1];
            i = 0;
            for (auto& var : environment) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
                c_env[i] = const_cast<char*>(var.c_str());
                i++;
            }
            c_env[i] = nullptr;

            // Handle capabilities and drop permissions
            if (!handle_caps()) {
                LOG->critical("Couldn't drop privileges as intended, aborting now!");
                exit(-1);
            }

            // Execute program
            int retval = execvpe(program, static_cast<char* const*>(c_args), static_cast<char* const*>(c_env));

            // If we get to this point something went wrong...
            LOG->critical("Could not exec child process: {0}", retval);
            exit(-1);
        }
        close(stdout[1]);
        close(stderr[1]);
        LOG->info("Child pid: {0}", primaryPid);
        state = RUNNING;
        reg[primaryPid] = graph_id;
    }

    void ChildProcess::register_with_epoll(int epoll_fd, std::map<int, unsigned int>& map,
                                           std::map<int, ProcessHandlerInterface::FDType>& fd_type) noexcept(false) {
        struct epoll_event setup {};
        setup.data.fd = stdout[0];
        setup.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stdout[0], &setup) == -1) {
            throw ChildProcessException("Couldn't bind stdout pipe to epoll socket!");
        }
        setup.data.fd = stderr[0];
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stderr[0], &setup) == -1) {
            throw ChildProcessException("Couldn't bind stderr pipe to epoll socket!");
        }
        map[stdout[0]] = graph_id;
        map[stderr[0]] = graph_id;
        fd_type[stdout[0]] = ProcessHandlerInterface::FDType::STDOUT;
        fd_type[stderr[0]] = ProcessHandlerInterface::FDType::STDERR;
    }

    std::string ChildProcess::get_name() const noexcept { return name; }

    bool ChildProcess::can_start_now() const noexcept { return state == READY; }

    void ChildProcess::should_wait_for(unsigned int other_process, ProcessState other_state) noexcept {
        bool contains =
          std::accumulate(conditions.begin(), conditions.end(), false, [other_process](bool contains, auto pair) {
              if (contains) {
                  return true;
              }
              return pair.first == other_process;
          });
        if (!contains) {
            LOG->debug("Proc {0}: Adding new dependency on proc {1}", this->name, other_process);
            conditions.emplace_back(std::make_pair(other_process, other_state));
        } else {
            LOG->debug("Proc {0}: Not adding new dependency on proc {1}", this->name, other_process);
        }
    }

    void ChildProcess::propagate_dependencies(
      std::list<std::weak_ptr<scinit::ChildProcessInterface>> other_processes) noexcept {
        auto func = [&other_processes, classref = this](auto dependency, bool other_or_this) {
            for (const auto& weak_ref : other_processes) {
                if (auto ref = weak_ref.lock()) {
                    if (ref->get_name() == dependency) {
                        if (other_or_this) {
                            ref->should_wait_for(classref->graph_id,
                                                 classref->type == SIMPLE ? ProcessState::DONE : ProcessState::RUNNING);
                        } else {
                            classref->should_wait_for(
                              ref->get_id(), classref->type == SIMPLE ? ProcessState::DONE : ProcessState::RUNNING);
                        }
                    }
                }
            }
        };
        std::for_each(before.begin(), before.end(), [&func](auto arg) { func(arg, true); });
        std::for_each(after.begin(), after.end(), [&func](auto arg) { func(arg, false); });
        before.clear();
        after.clear();
    }

    ChildProcessInterface::ProcessState ChildProcess::get_state() const noexcept { return state; }

    void ChildProcess::notify_of_state(
      std::map<unsigned int, std::weak_ptr<ChildProcessInterface>> other_procs) noexcept {
        if (state == BLOCKED) {
            bool still_blocked = std::accumulate(
              conditions.begin(), conditions.end(), false, [&other_procs](bool blocked, auto condition) {
                  if (blocked) {
                      return true;
                  }
                  if (!other_procs.count(condition.first)) {
                      LOG->critical("BUG: Found reference to process that doesn't exist");
                      return true;
                  }
                  if (auto ptr = other_procs[condition.first].lock()) {
                      auto retval = ptr->get_state() != condition.second;
                      if (!retval) {
                          LOG->debug("Condition {0} not in state {1} fulfilled with state {2}", ptr->get_name(),
                                  condition.second, ptr->get_state());
                      }
                      return retval;
                  }
                  LOG->critical("BUG: Found reference to process that doesn't exist anymore");
                  return true;
              });
            if (!still_blocked) {
                LOG->debug("Proc {0}: Transition to ready", this->name);
                state = READY;
            }
        }
    }

    void ChildProcess::handle_process_event(ProcessHandlerInterface::ProcessEvent event, int data) noexcept {
        switch (event) {
            case ProcessHandlerInterface::ProcessEvent::SIGHUP:
                break;
            case ProcessHandlerInterface::ProcessEvent::EXIT:
                if (state != RUNNING) {
                    LOG->warn("Child process object in state {0} notified of exit?", state);
                }
                if (data == 0) {
                    state = DONE;
                } else {
                    state = CRASHED;
                }
                break;
        }
    }
}  // namespace scinit