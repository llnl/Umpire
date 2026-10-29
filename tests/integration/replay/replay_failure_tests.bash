#!/bin/bash
##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set -euo pipefail

replay_tests_dir=$(cd "$1" && pwd)
testprogram=$replay_tests_dir/replay_failure_tests
topdir=$replay_tests_dir/..

cleanupandexit() {
  local status=$1
  local mydir
  mydir=$(pwd)
  cd "$topdir"
  find . -maxdepth 1 -name 'umpire.*.stats' | xargs -r rm -f
  cd "$mydir"
  exit "$status"
}

trap 'cleanupandexit 1' ERR

cd "$topdir"
find . -maxdepth 1 -name 'umpire.*.stats' | xargs -r rm -f

echo "UMPIRE_REPLAY='On' $testprogram"
UMPIRE_REPLAY="On" "$testprogram"

generated_trace=$(find . -maxdepth 1 -name 'umpire.*.stats' | head -n 1)
if [ -z "$generated_trace" ]; then
  echo "Failed: replay failure test binary did not generate a .stats file"
  cleanupandexit 1
fi

grep '"name":"bad_alignment_allocator".*"status":"pending"' "$generated_trace" >/dev/null
if grep '"name":"bad_alignment_allocator".*"status":"committed"' "$generated_trace" >/dev/null; then
  echo "Failed: constructor failure was incorrectly committed in replay trace"
  cleanupandexit 1
fi

limited_id=$(grep '"name":"size_limited_allocator".*"status":"committed"' "$generated_trace" | \
  sed -E 's/.*"allocator_id":"([^"]+)".*/\1/' | head -n 1)

if [ -z "$limited_id" ]; then
  echo "Failed: size-limited allocator creation was not committed"
  cleanupandexit 1
fi

grep "\"allocator_id\":\"$limited_id\"" "$generated_trace" | grep '"op":"allocate"' | grep '"size":2' | grep '"status":"pending"' >/dev/null
if grep "\"allocator_id\":\"$limited_id\"" "$generated_trace" | grep '"op":"allocate"' | grep '"size":2' | grep '"status":"committed"' >/dev/null; then
  echo "Failed: failed allocation was incorrectly committed in replay trace"
  cleanupandexit 1
fi

trap - ERR
cleanupandexit 0
