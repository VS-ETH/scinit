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

#ifndef CINIT_CHILDPROCESSEXCEPTION_H
#define CINIT_CHILDPROCESSEXCEPTION_H

#include <exception>
#include <string>

namespace scinit {
    class ChildProcessException : virtual std::exception {
      public:
        explicit ChildProcessException(const char*) noexcept;
        const char* what() const noexcept override;

      private:
        std::string reason;
    };
}  // namespace scinit

#endif  // CINIT_CHILDPROCESSEXCEPTION_H
