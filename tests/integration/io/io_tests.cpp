//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/util/MPI.hpp"
#include "umpire/util/io.hpp"

#if defined(UMPIRE_ENABLE_LOGGING)
#include "umpire/util/Logger.hpp"
#endif

#if defined(UMPIRE_ENABLE_MPI)
#include "mpi.h"
#endif

#include "umpire/CLI11/CLI11.hpp"

int main(int argc, char** argv)
{
#if defined(UMPIRE_ENABLE_MPI)
  MPI_Init(&argc, &argv);
  umpire::initialize(MPI_COMM_WORLD);
#else
  (void)argc;
  (void)argv;
  umpire::initialize();
#endif

  bool enable_logging{false};
  CLI::App app{"IO tests"};

  app.add_flag("-l,--enable-logging", enable_logging, "Enable logging output");

  CLI11_PARSE(app, argc, argv);

#if defined(UMPIRE_ENABLE_LOGGING)
  if (enable_logging) {
#if defined(_MSC_VER)
    _putenv_s("UMPIRE_LOG_LEVEL", "Info");
#else
    setenv("UMPIRE_LOG_LEVEL", "Info", 1);
#endif
    umpire::util::Logger::initialize();
    umpire::util::Logger::log(umpire::util::message::Info, "testing log stream", __FILE__, __LINE__);
    umpire::util::Logger::log(umpire::util::message::Error, "testing error stream", __FILE__, __LINE__);
    umpire::util::Logger::finalize();
  }
#else
  (void)enable_logging;
#endif

#if defined(UMPIRE_ENABLE_MPI)
  MPI_Finalize();
#endif

  return 0;
}
