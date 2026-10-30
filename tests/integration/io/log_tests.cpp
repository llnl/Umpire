//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/ResourceManager.hpp"
#include "umpire/util/Macros.hpp"

int main(int UMPIRE_UNUSED_ARG(argc), char** UMPIRE_UNUSED_ARG(argv))
{
  // This test exercises logging through the ResourceManager lifecycle
  // The ResourceManager constructor calls Logger::initialize()
  // The ResourceManager destructor calls Logger::finalize()
  auto& rm = umpire::ResourceManager::getInstance();

  // Perform some operations that will generate log messages
  auto alloc = rm.getAllocator("HOST");
  void* ptr = alloc.allocate(1024);

  UMPIRE_LOG(Info, "Test log message from log_tests");
  UMPIRE_LOG(Debug, "Debug message from log_tests");

  alloc.deallocate(ptr);

  // ResourceManager destructor will finalize logging
  return 0;
}
