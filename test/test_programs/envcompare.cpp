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
#include <map>

extern char** environ;
// Detect whether environment filtering works
int main(int, char**) {
    std::map<std::string, std::string> allowed_var_values;
    allowed_var_values["HOME"] = "/app";
    allowed_var_values["LANG"] = "C";
    allowed_var_values["LANGUAGE"] = "en";
    allowed_var_values["LOGNAME"] = "nobody";
    allowed_var_values["PATH"] = "/usr/local/bin:/usr/bin:/bin";
    allowed_var_values["PWD"] = "/app";
    allowed_var_values["SHELL"] = "/bin/bash";
    allowed_var_values["TERM"] = "screen";
    allowed_var_values["USER"] = "nobody";
    allowed_var_values["FOO"] = "bar";
    allowed_var_values["BAR"] = "nobody-bar";

    bool is_ok = true;
    for (int i = 0; environ[i] != nullptr; i++) {
        std::string var(environ[i]);
        std::cout << var << std::endl;
        auto pos = var.find('=');
        if (pos <= 0) {
            std::cerr << "ERROR: Couldn't find '=' in environment variable string '" << var << "'" << std::endl;
            return -1;
        }
        auto key = var.substr(0, pos);
        auto val = var.substr(pos+1);
        if (allowed_var_values.count(key) == 1) {
            auto allowed_val = allowed_var_values[key];
            if (val != allowed_val) {
                std::cerr << "EROR: Variable '" << key << "' has the wrong value!" << std::endl;
                is_ok = false;
            }
            allowed_var_values.erase(key);
        } else {
            std::cerr << "EROR: Variable '" << key << "' is not allowed!" << std::endl;
            is_ok = false;
        }
    }
    if (is_ok && allowed_var_values.empty()) {
        return 0;
    } else {
        return 1;
    }
}
