# scinit
[![CircleCI](https://circleci.com/gh/uubk/scinit/tree/master.svg?style=shield)](https://circleci.com/gh/uubk/cinit/tree/master)

A small init daemon for containers. Supervises multiple processes (optionally restarting them), forwards signals and handles capabilities.

## Why would I need this?
Because in some environments, it might not be desireable to only run a single, non-root process per container. Some services might need eleveated permissions (e.g. `CAP_NET_RAW`) and maybe you have another deployment option besides Kubernetes that you need to support, so splitting every app into multiple containers is just not possible.

## Building
You'll need CMake, libcap2, Boost::Program_Options and yaml-cpp. On Debian: `apt install libcap-dev libyaml-cpp-dev libboost-program-options-dev cmake`. Afterwards, it's
```
mkdir build
cd build
cmake ..
make -j<nprocs>
```

## How to use
You'll need to create a cinit configuration like this:
```
programs:
  - name: ping
    path: ./ping
    args:
      - -c 4
      - google.ch
    type: oneshot
    uid: 65534
    gid: 65534
    capabilities:
      - CAP_NET_RAW
```
This would start a `./ping -c 4 google.ch` a single time (`type: onshot`) as `nobody:nogroup` and grant it `CAP_NET_RAW` so that it works without root privileges. The only mandatory options from this example are `name` and `path`.

## Dependencies
This project depends on gtest+gmock and spdlog, which are pulled in via submodule.
Additionally, CMake's `GoogleTest` module is included for compatibility with earlier
CMake versions. To make unit test results appear in CircleCI, this project relies on the XSLT
from [https://stackoverflow.com/a/6329217](https://stackoverflow.com/a/6329217).