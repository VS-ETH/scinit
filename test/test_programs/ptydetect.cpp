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

#include <unistd.h>
#include <iostream>

// Detect whether stdout and stderr are ttys
int main(int, char**) {
    if (isatty(STDOUT_FILENO)) {
        std::cout << "stdout is a tty" << std::endl;
    } else {
        std::cout << "stdout is not a tty" << std::endl;
    }
    if (isatty(STDERR_FILENO)) {
        std::cerr << "stderr is a tty" << std::endl;
    } else {
        std::cerr << "stderr is not a tty" << std::endl;
    }
    return 0;
}
