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
#include <unistd.h>
#include <fstream>
#include <iostream>

int main(int, char**) {
    /*auto uid = getuid(), euid = geteuid(), gid = getgid(), egid = getegid();

    if (uid != euid || uid != 65534) {
        std::cerr << "Wrong user" << std:: endl;
        return -1;
    }
    if (gid != egid || gid != 65534) {
        std::cerr << "Wrong group" << std:: endl;
        return -1;
    }
    std::cout << "User/group ok" << std::endl;*/

    // Try to open /proc/self/fd/1, aka stdout. This will only work if permissions on the terminal have been set
    // correctly
    try {
        auto fd = fopen("/proc/self/fd/1", "w");
        if (fd == nullptr) {
            std::cerr << "Couldn't open FD!" << std::endl;
            return -1;
        }
        fprintf(fd, "Test via FD\n");
        fclose(fd);
        std::cout << "Test via stream" << std::endl;
    } catch (...) {
        std::cerr << "Couldn't open FD!" << std::endl;
        return -1;
    }
    return 0;
}
