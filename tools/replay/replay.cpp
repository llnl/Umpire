//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <ratio>
#include <string>
#include <vector>

#include "umpire/util/Macros.hpp"

#if !defined(_MSC_VER)
#include "ReplayInterpreter.hpp"
#include "ReplayOptions.hpp"
#include "umpire/CLI11/CLI11.hpp"

#endif // !defined(_MSC_VER)

int main(int argc, char* argv[])
{
#if !defined(_MSC_VER)
  try {
    ReplayOptions options;
    CLI::App app{"Replay an umpire session from a file"};
    app.add_option("-i,--infile", options.input_file, "Input file")->required()->check(CLI::ExistingFile);
    app.add_flag("-q,--quiet", options.quiet, "Only errors will be displayed.");
    app.add_flag("-t,--time-run", options.time_replay_run, "Display time information for replay running operations");
    app.add_flag("-d,--dump", options.dump_statistics, "Dump ULTRA memory usage trace for each Allocator");
    app.add_flag("-s,--stats", options.track_stats, "Track/Display pool allocation size statistics");
    app.add_flag("-r,--recompile", options.force_compile, "Accepted for compatibility; ignored by replay v2");
    CLI11_PARSE(app, argc, argv);

    std::chrono::high_resolution_clock::time_point t1;
    std::chrono::high_resolution_clock::time_point t2;
    std::chrono::duration<double> time_span;

    t1 = std::chrono::high_resolution_clock::now();
    ReplayInterpreter replay(options);

    replay.buildOperations();

    if (options.time_replay_parse) {
      t2 = std::chrono::high_resolution_clock::now();
      time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << "Parsing replay log took " << time_span.count() << " seconds." << std::endl;
    }

    t1 = std::chrono::high_resolution_clock::now();
    replay.runOperations();

    if (options.time_replay_run) {
      t2 = std::chrono::high_resolution_clock::now();
      time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << "Running replay took " << time_span.count() << " seconds." << std::endl;
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    return 1;
  }
#else
  UMPIRE_USE_VAR(argc);
  UMPIRE_USE_VAR(argv);
  std::cerr << "Replay tool is not supported on MSVC in this configuration." << std::endl;
#endif // !defined(_MSC_VER)
  return 0;
}
