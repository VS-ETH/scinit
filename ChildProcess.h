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

#ifndef CINIT_CHILDPROCESS_H
#define CINIT_CHILDPROCESS_H

#include <string>
#include <list>
#include <map>

namespace cinit {

    class ChildProcess : public std::enable_shared_from_this<ChildProcess> {
    public:
        ChildProcess(const std::string&, const std::string &, const std::list<std::string> &,
                        const std::string &, const std::list<std::string> &, int uid, int gid);
        void do_fork(std::map<int, std::shared_ptr<cinit::ChildProcess>>&) noexcept(false);
        int register_with_epoll(int, std::map<int, std::shared_ptr<ChildProcess>>&) noexcept(false);
        std::string get_name() const noexcept;
        bool should_restart() const noexcept;
        bool should_restart_now() noexcept;
        void notify_of_exit(int rc) noexcept;

        enum ProcessType {
            ONESHOT,
            SIMPLE
            // TODO (maybe): FORKING
        };

        enum ProcessState {
            READY,
            RUNNING,
            DONE,
            CRASHED,
            BACKOFF
        };

    private:
        std::string path, name;
        std::list<std::string> args, capabilities;
        int primaryPid, uid, gid;
        int stdouterr[2];
        ProcessType type;
        ProcessState state;

        void handle_caps();
    };
}

#endif //CINIT_CHILDPROCESS_H
