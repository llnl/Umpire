//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <iostream>

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/QuickPool.hpp"

//
// This example shows the introspection calls that remain available when
// Umpire is built with -DUMPIRE_ENABLE_INTROSPECTION_HEADER=On. In this
// mode, metadata is stored in a small header in front of each allocation
// instead of in the global allocation map, reducing the cost of allocate
// and deallocate while keeping the most useful introspection queries.
//
int main(int, char**)
{
  auto& rm = umpire::ResourceManager::getInstance();

  // _sphinx_tag_tut_header_alloc_start
  auto allocator = rm.getAllocator("HOST");
  auto pooled_allocator = rm.makeAllocator<umpire::strategy::QuickPool>("HEADER_POOL", allocator);

  double* data = static_cast<double*>(pooled_allocator.allocate(1024 * sizeof(double)));
  // _sphinx_tag_tut_header_alloc_end

  //
  // Size and allocator queries work on any base pointer by reading the
  // allocation header.
  //
  // _sphinx_tag_tut_header_query_start
  std::cout << "Size of allocation: " << pooled_allocator.getSize(data) << " bytes" << std::endl;
  std::cout << "Allocated by: " << rm.getAllocator(data).getName() << std::endl;
  // _sphinx_tag_tut_header_query_end

  //
  // The running per-allocator statistics are kept as counters and do not
  // depend on the allocation map, so they are always available.
  //
  // _sphinx_tag_tut_header_stats_start
  std::cout << "Current size: " << pooled_allocator.getCurrentSize() << " bytes" << std::endl;
  std::cout << "High watermark: " << pooled_allocator.getHighWatermark() << " bytes" << std::endl;
  std::cout << "Allocation count: " << pooled_allocator.getAllocationCount() << std::endl;
  // _sphinx_tag_tut_header_stats_end

  pooled_allocator.deallocate(data);

  return 0;
}
