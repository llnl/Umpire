//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef REPLAY_ReplayOptions_HPP
#define REPLAY_ReplayOptions_HPP

#include <string>

struct ReplayOptions {
  ReplayOptions() {};
  bool time_replay_run{false};    // -t,--time-run
  bool time_replay_parse{false};  // --time-parse
  bool dump_statistics{false};    // -d, --dump
  bool track_stats{false};        // -s, --stats
  bool force_compile{false};      // -r,--recompile
  bool quiet{false};              // -q,--quiet
  std::string input_file;         // -i,-infile input_file
};

#endif  // REPLAY_ReplayOptions_HPP
