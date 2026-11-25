//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <string>
#include <vector>

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/strategy/AlignedAllocator.hpp"
#include "umpire/strategy/AllocationAdvisor.hpp"
#include "umpire/strategy/AllocationPrefetcher.hpp"
#include "umpire/strategy/AllocationStrategy.hpp"
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/FixedPool.hpp"
#include "umpire/strategy/MixedPool.hpp"
#include "umpire/strategy/MonotonicAllocationStrategy.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"
#include "umpire/strategy/NamingShim.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/ResourceAwarePool.hpp"
#include "umpire/strategy/SizeLimiter.hpp"
#include "umpire/strategy/SlotPool.hpp"
#include "umpire/strategy/ThreadSafeAllocator.hpp"
#include "umpire/util/wrap_allocator.hpp"

#if defined(UMPIRE_ENABLE_NUMA)
#include "umpire/strategy/NumaPolicy.hpp"
#include "umpire/util/numa.hpp"
#endif

namespace replay_test {

const int TEST_ALLOCATIONS{3};
const std::size_t ALLOCATION_SIZE{32};

void testAllocation(std::string name)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto alloc = rm.getAllocator(name);

  for (int i = 0; i < TEST_ALLOCATIONS; ++i) {
    auto ptr = alloc.allocate(ALLOCATION_SIZE);
    alloc.deallocate(ptr);
  }

  try {
    auto dynamic_pool = umpire::util::unwrap_allocator<umpire::strategy::DynamicPoolList>(alloc);
    dynamic_pool->coalesce();
    alloc.release();
  } catch (...) {
  }
}

template <typename Strategy, bool intro, typename... Args>
void testAllocator(std::string name, Args&&... args)
{
  auto& rm = umpire::ResourceManager::getInstance();
  rm.makeAllocator<Strategy, intro>(name, args...);
  testAllocation(name);
}

static void runTest()
{
  auto& rm = umpire::ResourceManager::getInstance();

  std::vector<std::string> allocators;
  allocators.push_back("HOST");
#if defined(UMPIRE_ENABLE_DEVICE)
  allocators.push_back("DEVICE");
  for (int id = 0; id < rm.getNumDevices(); id++) {
    allocators.push_back(std::string{"DEVICE::" + std::to_string(id)});
  }
#endif
#if defined(UMPIRE_ENABLE_UM)
  allocators.push_back("UM");
#endif
#if defined(UMPIRE_ENABLE_PINNED)
  allocators.push_back("PINNED");
#endif

  for (auto basename : allocators) {
    testAllocation(basename);

    auto base_alloc = rm.getAllocator(basename);
    std::string name;

#if defined(UMPIRE_ENABLE_NUMA)
    if (basename == "HOST") {
      auto nodes = umpire::numa::get_host_nodes();

      name = basename + "_NumaPolicy";
      testAllocator<umpire::strategy::NumaPolicy, true>(name, rm.getAllocator("HOST"), nodes[0]);
      name = basename + "_NumaPolicy_no_introspection";
      testAllocator<umpire::strategy::NumaPolicy, false>(name, rm.getAllocator("HOST"), nodes[0]);
    }
#endif // defined(UMPIRE_ENABLE_NUMA)

#if defined(UMPIRE_ENABLE_CUDA)
    if (basename == "UM") {
      auto device_id = 1; // device_id
      auto accessing_alloc = rm.getAllocator("HOST");

      name = basename + "_AllocationAdvisor_spec_";
      testAllocator<umpire::strategy::AllocationAdvisor, true>(name + "default_id", base_alloc, "SET_READ_MOSTLY");
      testAllocator<umpire::strategy::AllocationAdvisor, true>(name + "with_id", base_alloc, "SET_READ_MOSTLY",
                                                               device_id);
      testAllocator<umpire::strategy::AllocationAdvisor, true>(name + "with_accessing_and_default_id", base_alloc,
                                                               "SET_READ_MOSTLY", accessing_alloc);
      testAllocator<umpire::strategy::AllocationAdvisor, true>(name + "with_accessing_and_id", base_alloc,
                                                               "SET_PREFERRED_LOCATION", accessing_alloc, device_id);
      name = basename + "_AllocationAdvisor_no_introspection_spec_";
      testAllocator<umpire::strategy::AllocationAdvisor, false>(name + "default_id", base_alloc, "SET_READ_MOSTLY");
      testAllocator<umpire::strategy::AllocationAdvisor, false>(name + "with_id", base_alloc, "SET_READ_MOSTLY",
                                                                device_id);
      testAllocator<umpire::strategy::AllocationAdvisor, false>(name + "with_accessing_and_default_id", base_alloc,
                                                                "SET_READ_MOSTLY", accessing_alloc);
      testAllocator<umpire::strategy::AllocationAdvisor, false>(name + "with_accessing_and_id", base_alloc,
                                                                "SET_PREFERRED_LOCATION", accessing_alloc, device_id);

      name = basename + "_AllocationPrefetcher";
      testAllocator<umpire::strategy::AllocationPrefetcher, true>(name, base_alloc);
      name = basename + "_AllocationPrefetcher_no_introspection";
      testAllocator<umpire::strategy::AllocationPrefetcher, false>(name, base_alloc);
    }
#endif // defined(UMPIRE_ENABLE_CUDA)

    auto mpa1 = 512;             // Smallest fixed pool object size in bytes
    auto mpa2 = 1 * 1024;        // Largest fixed pool object size in bytes
    auto mpa3 = 8 * 1024;        // Largest initial size of any fixed pool
    auto mpa4 = 12;              // Fixed pool object size increase factor
    auto mpa5 = 256 * 1024;      // Size the dynamic pool initially allocates
    auto mpa6 = 1 * 1024 * 1024; // Minimum size of all future allocations in the dynamic pool
    auto mpa7 = 128;             // Size with which to align allocations
    name = basename + "_MixedPool_spec_";
    testAllocator<umpire::strategy::MixedPool, true>(name + "0", base_alloc);
    testAllocator<umpire::strategy::MixedPool, true>(name + "1", base_alloc, mpa1);
    testAllocator<umpire::strategy::MixedPool, true>(name + "2", base_alloc, mpa1, mpa2);
    testAllocator<umpire::strategy::MixedPool, true>(name + "3", base_alloc, mpa1, mpa2, mpa3);
    testAllocator<umpire::strategy::MixedPool, true>(name + "4", base_alloc, mpa1, mpa2, mpa3, mpa4, mpa5);
    testAllocator<umpire::strategy::MixedPool, true>(name + "5", base_alloc, mpa1, mpa2, mpa3, mpa4, mpa5);
    testAllocator<umpire::strategy::MixedPool, true>(name + "6", base_alloc, mpa1, mpa2, mpa3, mpa4, mpa5, mpa6);
    testAllocator<umpire::strategy::MixedPool, true>(name + "7", base_alloc, mpa1, mpa2, mpa3, mpa4, mpa5, mpa6, mpa7);
    name = basename + "_MixedPool_no_instrospection_spec_";
    testAllocator<umpire::strategy::MixedPool, false>(name + "0", base_alloc);
    testAllocator<umpire::strategy::MixedPool, false>(name + "1", base_alloc, mpa1);
    testAllocator<umpire::strategy::MixedPool, false>(name + "2", base_alloc, mpa1, mpa2);
    testAllocator<umpire::strategy::MixedPool, false>(name + "3", base_alloc, mpa1, mpa2, mpa3);
    testAllocator<umpire::strategy::MixedPool, false>(name + "4", base_alloc, mpa1, mpa2, mpa3, mpa4, mpa5);
    testAllocator<umpire::strategy::MixedPool, false>(name + "5", base_alloc, mpa1, mpa2, mpa3, mpa4, mpa5);
    testAllocator<umpire::strategy::MixedPool, false>(name + "6", base_alloc, mpa1, mpa2, mpa3, mpa4, mpa5, mpa6);
    testAllocator<umpire::strategy::MixedPool, false>(name + "7", base_alloc, mpa1, mpa2, mpa3, mpa4, mpa5, mpa6, mpa7);

    auto pa1 = 16 * 1024; // min initial allocation size
    auto pa2 = 1 * 1024;  // min allocation size
    auto pa3 = 128;
    auto pa4 = umpire::strategy::QuickPool::percent_releasable(50);
    auto rpa4 = umpire::strategy::ResourceAwarePool::percent_releasable(50);
    name = basename + "_Pool_spec_";
    testAllocator<umpire::strategy::QuickPool, true>(name + "0", base_alloc);
    testAllocator<umpire::strategy::QuickPool, true>(name + "1", base_alloc, pa1);
    testAllocator<umpire::strategy::QuickPool, true>(name + "2", base_alloc, pa1, pa2);
    testAllocator<umpire::strategy::QuickPool, true>(name + "3", base_alloc, pa1, pa2, pa3);
    testAllocator<umpire::strategy::QuickPool, true>(name + "4", base_alloc, pa1, pa2, pa3, pa4);
    name = basename + "_Pool_no_instrospection_spec_";
    testAllocator<umpire::strategy::QuickPool, false>(name + "0", base_alloc);
    testAllocator<umpire::strategy::QuickPool, false>(name + "1", base_alloc, pa1);
    testAllocator<umpire::strategy::QuickPool, false>(name + "2", base_alloc, pa1, pa2);
    testAllocator<umpire::strategy::QuickPool, false>(name + "3", base_alloc, pa1, pa2, pa3);
    testAllocator<umpire::strategy::QuickPool, false>(name + "4", base_alloc, pa1, pa2, pa3, pa4);

    auto lpa1 = 16 * 1024; // min initial allocation size
    auto lpa2 = 1 * 1024;  // min allocation size
    auto lpa3 = 128;       // byte alignment
    auto lpa4 = umpire::strategy::DynamicPoolList::percent_releasable(50);
    name = basename + "_DynamicPoolList_spec_";
    testAllocator<umpire::strategy::DynamicPoolList, true>(name + "0", base_alloc);
    testAllocator<umpire::strategy::DynamicPoolList, true>(name + "1", base_alloc, lpa1);
    testAllocator<umpire::strategy::DynamicPoolList, true>(name + "2", base_alloc, lpa1, lpa2);
    testAllocator<umpire::strategy::DynamicPoolList, true>(name + "3", base_alloc, lpa1, lpa2, lpa3);
    testAllocator<umpire::strategy::DynamicPoolList, true>(name + "4", base_alloc, lpa1, lpa2, lpa3, lpa4);
    name = basename + "_DynamicPoolList_no_instrospection_spec_";
    testAllocator<umpire::strategy::DynamicPoolList, false>(name + "0", base_alloc);
    testAllocator<umpire::strategy::DynamicPoolList, false>(name + "1", base_alloc, lpa1);
    testAllocator<umpire::strategy::DynamicPoolList, false>(name + "2", base_alloc, lpa1, lpa2);
    testAllocator<umpire::strategy::DynamicPoolList, false>(name + "3", base_alloc, lpa1, lpa2, lpa3);
    testAllocator<umpire::strategy::DynamicPoolList, false>(name + "4", base_alloc, lpa1, lpa2, lpa3, lpa4);

    auto ma1 = 1024; // Capacity
    name = basename + "_MonotonicAllocationStrategy_spec_";
    testAllocator<umpire::strategy::MonotonicAllocationStrategy, true>(name, base_alloc, ma1);
    name = basename + "_MonotonicAllocationStrategy_no_instrospection_spec_";
    testAllocator<umpire::strategy::MonotonicAllocationStrategy, false>(name, base_alloc, ma1);

    name = basename + "_NamedAllocationStrategy_spec_";
    testAllocator<umpire::strategy::NamedAllocationStrategy, true>(name, base_alloc);
    name = basename + "_NamedAllocationStrategy_no_instrospection_spec_";
    testAllocator<umpire::strategy::NamedAllocationStrategy, false>(name, base_alloc);

    name = basename + "_NamingShim_spec_";
    testAllocator<umpire::strategy::NamingShim, true>(name, base_alloc);
    name = basename + "_NamingShim_no_instrospection_spec_";
    testAllocator<umpire::strategy::NamingShim, false>(name, base_alloc);

    name = basename + "_AlignedAllocator_spec_";
    testAllocator<umpire::strategy::AlignedAllocator, true>(name, base_alloc, 64);
    name = basename + "_AlignedAllocator_no_instrospection_spec_";
    testAllocator<umpire::strategy::AlignedAllocator, false>(name, base_alloc, 64);

    name = basename + "_SizeLimiter_spec_";
    testAllocator<umpire::strategy::SizeLimiter, true>(name, base_alloc, 1024);
    name = basename + "_SizeLimiter_no_instrospection_spec_";
    testAllocator<umpire::strategy::SizeLimiter, false>(name, base_alloc, 1024);

    auto sa1 = 64; // Slots
    name = basename + "_SlotPool_spec_";
    testAllocator<umpire::strategy::SlotPool, true>(name, base_alloc, sa1);
    name = basename + "_SlotPool_no_instrospection_spec_";
    testAllocator<umpire::strategy::SlotPool, false>(name, base_alloc, sa1);

    name = basename + "_ThreadSafeAllocator_spec_";
    testAllocator<umpire::strategy::ThreadSafeAllocator, true>(name, base_alloc);
    name = basename + "_ThreadSafeAllocator_no_instrospection_spec_";
    testAllocator<umpire::strategy::ThreadSafeAllocator, false>(name, base_alloc);

    name = basename + "_ResourceAwarePool_spec_";
    testAllocator<umpire::strategy::ResourceAwarePool, true>(name + "0", base_alloc);
    testAllocator<umpire::strategy::ResourceAwarePool, true>(name + "1", base_alloc, pa1);
    testAllocator<umpire::strategy::ResourceAwarePool, true>(name + "2", base_alloc, pa1, pa2);
    testAllocator<umpire::strategy::ResourceAwarePool, true>(name + "3", base_alloc, pa1, pa2, pa3);
    testAllocator<umpire::strategy::ResourceAwarePool, true>(name + "4", base_alloc, pa1, pa2, pa3, rpa4);
    name = basename + "_ResourceAwarePool_no_instrospection_spec_";
    testAllocator<umpire::strategy::ResourceAwarePool, false>(name + "0", base_alloc);
    testAllocator<umpire::strategy::ResourceAwarePool, false>(name + "1", base_alloc, pa1);
    testAllocator<umpire::strategy::ResourceAwarePool, false>(name + "2", base_alloc, pa1, pa2);
    testAllocator<umpire::strategy::ResourceAwarePool, false>(name + "3", base_alloc, pa1, pa2, pa3);
    testAllocator<umpire::strategy::ResourceAwarePool, false>(name + "4", base_alloc, pa1, pa2, pa3, rpa4);

    auto fpa1 = ALLOCATION_SIZE; // object_bytes
    auto fpa2 = 1024;            // objects_per_pool
    name = basename + "_FixedPool_spec_";
    testAllocator<umpire::strategy::FixedPool, true>(name + "1", base_alloc, fpa1);
    testAllocator<umpire::strategy::FixedPool, true>(name + "2", base_alloc, fpa1, fpa2);
    name = basename + "_FixedPool_no_instrospection_spec_";
    testAllocator<umpire::strategy::FixedPool, false>(name + "1", base_alloc, fpa1);
    testAllocator<umpire::strategy::FixedPool, false>(name + "2", base_alloc, fpa1, fpa2);
  }

}

} // namespace replay_test

int main(int, char**)
{
  replay_test::runTest();

  return 0;
}
