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

#include <iostream>
#include <string>
#include <unordered_set>

extern char** environ;
// Detect whether environment filtering works
int main(int, char**) {
    std::unordered_set<std::string> allowed_vars = {"HOME", "LANG",  "LANGUAGE", "LOGNAME", "PATH",
                                                    "PWD",  "SHELL", "TERM",     "USER"};
    bool is_ok = true;
    for (int i = 0; environ[i] != nullptr; i++) {
        std::string var(environ[i]);
        std::cout << var << std::endl;
        auto pos = var.find('=');
        if (pos <= 0) {
            std::cerr << "ERROR: Coudln't find '=' in environment variable string '" << var << "'" << std::endl;
            return -1;
        }
        auto key = var.substr(0, pos);
        if (allowed_vars.erase(key) != 1) {
            std::cerr << "WARINIG: Invalid variable: '" << key << "'" << std::endl;
            is_ok = false;
        }
    }
    if (is_ok && allowed_vars.empty()) {
        return 0;
    } else {
        return 1;
    }
}
