---
name: building
description: Instructions for building Umpire
---

# Building

Umpire uses a make-based system for building. Umpire has a few submodules which should be up-to-date (run `git submodule update --init --recursive` to make sure). Building should always happen in a separate `build` directory unless otherwise noted.

The most common build recipe is to run `make -j`. If there is an error in the build, you can rerun with `make VERBOSE=1` to get more information. A summary of this verbose output should be given to the user.

To clean a build run `make clean`.

Try to build with more recent versions of compilers if possible. For example, building with `gcc` version 10.3.1 is better than building with 8.3.1. If you see you are using a very old version of a compiler, notify the user. For example, if you are using `gcc` version 4.9.3, notify the user right away!

## Common Build Configurations

Although Umpire should build from the `build` directory with just `cmake ../`, there are a few common build configurations that should always work. From within the `build` directory, common cmake commands are things like:

- `cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ../`
  - This will build Umpire with defaults using the `gcc` compiler. Other common compilers to use include `clang`.
- `cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DENABLE_DEVELOPER_DEFAULTS=On ../`
  - This will build Umpire with the clang sanitizer trying to detect memory leaks.

If you need more information about cmake configuration options, be sure to check out the documentation at `https://umpire.readthedocs.io/en/develop/sphinx/advanced_configuration.html`.

## Builds should not take a long time

If the build is taking a long time, notify the user of your cmake command and build configuration. Umpire is relatively simple and typically does not involve a long build time.

## Configuration Errors

If you are building Umpire and see a CMake configuration error, try running `make clean` and reattempt the build. You can also try deleting the CMakeCache.txt file to regenerate the cmake.
