#!/bin/bash

set -euo pipefail

git config --global --add safe.directory /github/workspace
git config --global --add safe.directory /github/workspace/.radiuss-ci
git config --global --add safe.directory /github/workspace/blt
git config --global --add safe.directory /github/workspace/scripts/radiuss-spack-configs
git config --global --add safe.directory /github/workspace/scripts/uberenv
git config --global --add safe.directory /github/workspace/src/tpl/umpire/camp
git config --global --add safe.directory /github/workspace/src/tpl/umpire/fmt

git submodule update --init --recursive

mkdir build && cd build 
cmake -DCMAKE_CXX_COMPILER=clang++ -DSHROUD_EXECUTABLE=/usr/local/bin/shroud ..
make -j 3 generate_umpire_shroud
