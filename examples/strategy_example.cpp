//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <iostream>

#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/MonotonicAllocationStrategy.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/SlotPool.hpp"

int main(int, char**)
{
  auto& rm = umpire::ResourceManager::getInstance();

  std::cout << "Available allocators: ";
  for (auto s : rm.getAllocatorNames()) {
    std::cout << s << "  ";
  }
  std::cout << std::endl;

  /*
   * Build some new Allocators from the named AllocationStrategies.
   *
   *  Named Allocators are stored in a map, and can be later accessed using the
   *  getAllocator function.
   */
  umpire::Tracking tracking{umpire::Tracking::Untracked};
  auto alloc = rm.makeAllocator<umpire::strategy::QuickPool>("host_quick_pool", tracking, rm.getAllocator("HOST"));

  alloc =
      rm.makeAllocator<umpire::strategy::MonotonicAllocationStrategy>("MONOTONIC 1024", rm.getAllocator("HOST"), 1024);

  alloc =
      rm.makeAllocator<umpire::strategy::MonotonicAllocationStrategy>("MONOTONIC 4096", rm.getAllocator("HOST"), 4096);

  alloc = rm.makeAllocator<umpire::strategy::SlotPool>("host_slot_pool", rm.getAllocator("HOST"), 64);

  /*
   * Get the previously created POOL allocator.
   *
   * Allocator getAllocator(const std::string& name); // name of allocator
   *
   */
  alloc = rm.getAllocator("host_slot_pool");
  void* test = alloc.allocate(100);
  alloc.deallocate(test);

  /*
   * Get the default HOST allocator.
   */
  alloc = rm.getAllocator("HOST");
  test = alloc.allocate(100);
  alloc.deallocate(test);

  /*
   * Get the previously created MONOTONIC allocator..
   */
  alloc = rm.getAllocator("MONOTONIC 1024");
  test = alloc.allocate(14);

  std::cout << "Size: " << alloc.getSize(test) << std::endl;
  alloc.deallocate(test);

  std::cout << "Available allocators: ";
  for (auto s : rm.getAllocatorNames()) {
    std::cout << s << ", ";
  }
  std::cout << std::endl;

  return 0;
}
