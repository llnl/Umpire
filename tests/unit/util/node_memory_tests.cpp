//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/util/node_memory.hpp"

TEST(NodeMemory, GetNodeAvailableMemory)
{
  // Test with default parameter
  double available_memory = umpire::util::get_node_available_memory();

#if defined(__linux__)
  // On Linux, we expect a positive value (unless /proc/meminfo is not accessible)
  // The function should return available memory in MiB
  // We'll accept any non-negative value as valid since memory availability varies
  EXPECT_GE(available_memory, 0.0);

  // Sanity check: available memory should be less than 1 TiB (1024 * 1024 MiB)
  // This is a reasonable upper bound for current systems
  EXPECT_LT(available_memory, 1024.0 * 1024.0);
#else
  // On non-Linux systems, the function should return the default value
  EXPECT_EQ(available_memory, -1.0);
#endif
}

TEST(NodeMemory, GetNodeAvailableMemoryWithCustomDefault)
{
  // Test with custom default value
  double custom_default = 12345.67;
  double available_memory = umpire::util::get_node_available_memory(custom_default);

#if defined(__linux__)
  // On Linux, we expect a positive value (actual available memory)
  // It should NOT be the custom default
  EXPECT_GE(available_memory, 0.0);
  EXPECT_NE(available_memory, custom_default);
#else
  // On non-Linux systems, it should return the custom default
  EXPECT_EQ(available_memory, custom_default);
#endif
}
