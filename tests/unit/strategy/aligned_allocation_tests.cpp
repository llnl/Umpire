//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <limits>

#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/util/error.hpp"

template <typename Pool>
static void check_rounding_overflow(const std::string& name)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto pool = rm.makeAllocator<Pool>(name, rm.getAllocator("HOST"));

  void* ptr{nullptr};
  ASSERT_NO_THROW(ptr = pool.allocate(64));

  ASSERT_THROW(pool.allocate(std::numeric_limits<std::size_t>::max()), umpire::runtime_error);
  ASSERT_THROW(pool.allocate(std::numeric_limits<std::size_t>::max() - 1), umpire::runtime_error);

  ASSERT_NO_THROW(pool.deallocate(ptr));
}

TEST(AlignedAllocationTest, QuickPoolRoundingOverflow)
{
  check_rounding_overflow<umpire::strategy::QuickPool>("quick_pool_rounding_overflow");
}

TEST(AlignedAllocationTest, DynamicPoolListRoundingOverflow)
{
  check_rounding_overflow<umpire::strategy::DynamicPoolList>("dynamic_pool_list_rounding_overflow");
}
