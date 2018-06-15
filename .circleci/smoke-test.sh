#!/bin/bash

export LANG=C
set -e

LD_LIBRARY_PATH="$(pwd)/build" build/cinit | tee log.txt

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
cat log.txt | grep '\[failping\]' | grep 'ping: socket: Operation not permitted' >> /dev/null
echo "ok"