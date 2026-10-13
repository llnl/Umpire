//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <cstdlib>

#include "benchmark/benchmark.h"

#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"
#include "umpire/util/AllocationHeader.hpp"
#include "umpire/util/AllocationMap.hpp"

//
// These benchmarks compare the cost of the two introspection mechanisms:
// the AllocationMap used by default, and the allocation header used when
// Umpire is built with -DUMPIRE_ENABLE_INTROSPECTION_HEADER=On. Both
// mechanisms are compiled into the library, so a single binary measures
// them side by side. The Allocator benchmarks at the end measure the full
// allocate/getSize/deallocate path with whichever mechanism this build of
// Umpire was configured to use.
//

static const int MAX_ALLOCATIONS = 100000;
static const std::size_t ALLOCATION_SIZE = 256;

class IntrospectionMechanism : public ::benchmark::Fixture {
 public:
  void SetUp(const ::benchmark::State&) override final
  {
    for (int i{0}; i < MAX_ALLOCATIONS; ++i) {
      m_allocations[i] = std::malloc(ALLOCATION_SIZE + umpire::util::allocation_header_size);
    }
  }

  void TearDown(const ::benchmark::State&) override final
  {
    for (int i{0}; i < MAX_ALLOCATIONS; ++i) {
      std::free(m_allocations[i]);
    }
  }

  void* m_allocations[MAX_ALLOCATIONS];
};

BENCHMARK_F(IntrospectionMechanism, MapInsertRemove)(benchmark::State& st)
{
  umpire::util::AllocationMap map;

  int i{0};
  while (st.KeepRunning()) {
    if (i == MAX_ALLOCATIONS) {
      st.PauseTiming();
      for (int j{0}; j < MAX_ALLOCATIONS; ++j) {
        map.remove(m_allocations[j]);
      }
      i = 0;
      st.ResumeTiming();
    }
    map.insert(m_allocations[i], {m_allocations[i], ALLOCATION_SIZE, nullptr});
    ++i;
  }

  st.PauseTiming();
  for (int j{0}; j < i; ++j) {
    map.remove(m_allocations[j]);
  }
  st.ResumeTiming();
}

BENCHMARK_F(IntrospectionMechanism, MapFind)(benchmark::State& st)
{
  umpire::util::AllocationMap map;
  for (int i{0}; i < MAX_ALLOCATIONS; ++i) {
    map.insert(m_allocations[i], {m_allocations[i], ALLOCATION_SIZE, nullptr});
  }

  int i{0};
  while (st.KeepRunning()) {
    if (i == MAX_ALLOCATIONS) {
      i = 0;
    }
    benchmark::DoNotOptimize(map.find(m_allocations[i++]));
  }

  for (int j{0}; j < MAX_ALLOCATIONS; ++j) {
    map.remove(m_allocations[j]);
  }
}

BENCHMARK_F(IntrospectionMechanism, HeaderWrite)(benchmark::State& st)
{
  int i{0};
  while (st.KeepRunning()) {
    if (i == MAX_ALLOCATIONS) {
      i = 0;
    }
    benchmark::DoNotOptimize(umpire::util::write_allocation_header(m_allocations[i++], ALLOCATION_SIZE, nullptr));
  }
}

BENCHMARK_F(IntrospectionMechanism, HeaderRead)(benchmark::State& st)
{
  void* user_ptrs[MAX_ALLOCATIONS];
  for (int i{0}; i < MAX_ALLOCATIONS; ++i) {
    user_ptrs[i] = umpire::util::write_allocation_header(m_allocations[i], ALLOCATION_SIZE, nullptr);
  }

  int i{0};
  while (st.KeepRunning()) {
    if (i == MAX_ALLOCATIONS) {
      i = 0;
    }
    benchmark::DoNotOptimize(umpire::util::get_allocation_header(user_ptrs[i++])->size);
  }
}

//
// End-to-end measurement of the introspection mechanism this build was
// configured with.
//
static void benchmark_allocator(benchmark::State& st, const std::string& name)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator(name);
  const std::size_t size{static_cast<std::size_t>(st.range(0))};

  while (st.KeepRunning()) {
    void* data = allocator.allocate(size);
    benchmark::DoNotOptimize(allocator.getSize(data));
    allocator.deallocate(data);
  }
}

static void AllocateGetSizeDeallocateHost(benchmark::State& st)
{
  benchmark_allocator(st, "HOST");
}

BENCHMARK(AllocateGetSizeDeallocateHost)->Arg(64)->Arg(4096)->Arg(1024 * 1024);

BENCHMARK_MAIN();
