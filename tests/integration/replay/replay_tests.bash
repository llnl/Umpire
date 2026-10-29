#!/bin/bash
##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
set -euo pipefail

replay_tests_dir=$1
tools_dir=$2
testprogram=$replay_tests_dir/replay_tests
diffprogram=$tools_dir/replaydiff
replayprogram=$tools_dir/replay
topdir=$tools_dir/..

cleanupandexit() {
  local status=$1
  local mydir
  mydir=$(pwd)
  cd "$topdir"
  find . -name '*.stats' -o -name 'replay*.ult' | xargs -r rm -f
  cd "$mydir"
  exit "$status"
}

trap 'cleanupandexit 1' ERR

cd "$topdir"
find . -name '*.stats' -o -name 'replay*.ult' | xargs -r rm -f

echo "UMPIRE_REPLAY='On' $testprogram"
UMPIRE_REPLAY="On" "$testprogram"

generated_trace=$(find . -maxdepth 1 -name 'umpire.*.stats' | head -n 1)
if [ -z "$generated_trace" ]; then
  echo "Failed: replay test binary did not generate a .stats file"
  cleanupandexit 1
fi

header=$(head -n 1 "$generated_trace")
echo "$header" | grep '"kind":"umpire_replay"' >/dev/null
echo "$header" | grep '"schema":"v2"' >/dev/null
grep '"status":"pending"' "$generated_trace" >/dev/null
grep '"status":"committed"' "$generated_trace" >/dev/null

mv "$generated_trace" replay.original.stats

echo "UMPIRE_REPLAY='On' $replayprogram -d -q -i replay.original.stats"
UMPIRE_REPLAY="On" "$replayprogram" -d -q -i replay.original.stats

replayed_trace=$(find . -maxdepth 1 -name 'umpire.*.stats' | head -n 1)
if [ -z "$replayed_trace" ]; then
  echo "Failed: replay tool did not generate a replayed .stats file"
  cleanupandexit 1
fi

if [ ! -s "$(find . -maxdepth 1 -name 'replay*.ult' | head -n 1)" ]; then
  echo "Failed: replay tool did not emit a non-empty .ult file"
  cleanupandexit 1
fi

echo "$diffprogram -q replay.original.stats $replayed_trace"
"$diffprogram" -q replay.original.stats "$replayed_trace"

printf '%s\n' \
  '{"kind":"umpire_replay","schema":"v2","process":{"pid":1,"rank":0},"umpire_version":"test"}' \
  '{"op":"make_allocator","seq":1,"allocator_id":"a1","name":"HOST","strategy":"MemoryResource","tracking":true,"args":{"resource_name":"HOST","traits":{"unified":false,"ipc":false,"size":0,"vendor":"unknown","kind":"unknown","used_for":"any","resource":"host","scope":"unknown","granularity":"unknown","tracking":true}},"status":"pending"}' \
  '{"op":"make_allocator","seq":1,"allocator_id":"a1","name":"HOST","strategy":"MemoryResource","tracking":true,"args":{"resource_name":"HOST","traits":{"unified":false,"ipc":false,"size":0,"vendor":"unknown","kind":"unknown","used_for":"any","resource":"host","scope":"unknown","granularity":"unknown","tracking":true}},"status":"committed"}' \
  '{"op":"allocate","seq":2,"allocator_id":"a1","allocation_id":"m1","size":32,"status":"pending"}' \
  > replay.pending.stats

"$replayprogram" -q -i replay.pending.stats

printf '%s\n' \
  '{"kind":"umpire_replay","schema":"v2","process":{"pid":1,"rank":0},"umpire_version":"test"}' \
  '{"op":"make_allocator","seq":1,"allocator_id":"a1","name":"HOST","strategy":"MemoryResource","tracking":true,"args":{"resource_name":"HOST","traits":{"unified":false,"ipc":false,"size":0,"vendor":"unknown","kind":"unknown","used_for":"any","resource":"host","scope":"unknown","granularity":"unknown","tracking":true}},"status":"pending"}' \
  '{"op":"make_allocator","seq":1,"allocator_id":"a1","name":"HOST","strategy":"MemoryResource","tracking":true,"args":{"resource_name":"HOST","traits":{"unified":false,"ipc":false,"size":0,"vendor":"unknown","kind":"unknown","used_for":"any","resource":"host","scope":"unknown","granularity":"unknown","tracking":true}},"status":"committed"}' \
  '{"op":"allocate","seq":2,"allocator_id":"a1","allocation_id":"m1","size":32,"status":"committed"}' \
  > replay.invalid.stats

trap - ERR
set +e
"$replayprogram" -q -i replay.invalid.stats >/dev/null 2>&1
invalid_status=$?
set -e
trap 'cleanupandexit 1' ERR

if [ "$invalid_status" -eq 0 ]; then
  echo "Failed: replay accepted a committed command without a pending lifecycle record"
  cleanupandexit 1
fi

trap - ERR
cleanupandexit 0
