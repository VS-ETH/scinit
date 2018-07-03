#!/bin/bash
#
# Copyright 2018 The scinit authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

export LANG=C
set -e

cp /bin/ping .

LD_LIBRARY_PATH="$(pwd)/build" build/scinit | tee log.txt

echo "Checking ping output"
cat log.txt | grep '\[ping\]' >> /dev/null
echo "ok"
echo "Checking detailed ping output"
cat log.txt | grep '\[ping\]' | grep "64 bytes" >> /dev/null
cat log.txt | grep '\[ping\]' | grep "'CAP_NET_RAW' enabled" >> /dev/null
echo "ok"

echo "Checking failping output"
cat log.txt | grep '\[failping\]' >> /dev/null
echo "ok"
echo "Checking detailed failping output"
# Debian 9:
# cat log.txt | grep '\[failping\]' | grep 'ping: socket: Operation not permitted' >> /dev/null
# Ubuntu 14.04 with swapped kernel:
cat log.txt | grep '\[failping\]' | grep 'ping: icmp open socket: Operation not permitted' >> /dev/null
echo "ok"
