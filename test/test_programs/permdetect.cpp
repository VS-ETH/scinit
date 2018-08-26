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

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

int main(int, char**) {
    auto uid = getuid(), euid = geteuid(), gid = getgid(), egid = getegid();

    auto nogroup = getgrnam("nogroup");
    if (!nogroup) {
        std::cerr << "Couldn't read group" << std::endl;
        return -1;
    }
    auto nobody = getpwnam("nobody");
    if (!nobody) {
        std::cerr << "Couldn't read user" << std::endl;
        return -1;
    }

    if (uid != euid || uid != nobody->pw_uid) {
        std::cerr << "Wrong user" << std::endl;
        return -1;
    }
    if (gid != egid || gid != nogroup->gr_gid) {
        std::cerr << "Wrong group" << std::endl;
        return -1;
    }
    std::cout << "User/group ok" << std::endl;
    return 0;
}
