//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/Umpire.hpp"
#include "umpire/ResourceManager.hpp"

#include <vector>

TEST(Umpire, ProcessorMemoryStatistics)
{
  ASSERT_GE(umpire::get_process_memory_usage(), 0);
  ASSERT_GE(umpire::get_mapping_memory_usage(""), 0);
  ASSERT_GE(umpire::get_device_memory_usage(0), 0);
}

TEST(Umpire, InternalMemoryUsage)
{
  std::size_t initial_usage = umpire::get_internal_memory_usage();
  ASSERT_GT(initial_usage, 0);
  ASSERT_LT(initial_usage, 1000000000);

  auto& rm = umpire::ResourceManager::getInstance();
  auto alloc = rm.getAllocator("HOST");

  std::vector<void*> allocations;
  for (int i = 0; i < 100; ++i) {
    allocations.push_back(alloc.allocate(1024));
  }

  std::size_t usage_after_allocs = umpire::get_internal_memory_usage();
  ASSERT_GT(usage_after_allocs, 0);
  ASSERT_LT(usage_after_allocs, 1000000000);

  for (auto ptr : allocations) {
    alloc.deallocate(ptr);
  }

  std::size_t final_usage = umpire::get_internal_memory_usage();
  ASSERT_GT(final_usage, 0);
  ASSERT_LT(final_usage, 1000000000);
}
