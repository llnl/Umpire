//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <unistd.h>

#include "gtest/gtest.h"
#include "mpi.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/resource/HostSharedMemoryResource.hpp"
#include "umpire/util/MemoryResourceTraits.hpp"

namespace {
struct SharedMemoryState {
  std::size_t largest_allocation_size;
  std::size_t initial_size;
  std::size_t num_allocations;
  std::size_t allocation_sizes[1];
};
const std::string shmem_state_name{"SharedMemoryState"};
// Maximum IPC allocation size depends on the stored allocation name length.
const std::string max_allocation_name{"AllocLargest"};
const std::string allocator_name{"SHARED::POSIX::node_allocator"};
SharedMemoryState* shmem_state{nullptr};

umpire::Allocator allocator;
umpire::resource::HostSharedMemoryResource* shmem_resource{nullptr};

class SharedMemoryTest : public ::testing::Test {
 public:
  using ArrayElement = int;

  const std::size_t m_segment_size{512ULL * 1024ULL * 1024ULL};
  const std::size_t m_max_allocs{1024};
  const std::size_t m_max_elems{m_segment_size / (m_max_allocs * sizeof(ArrayElement))};
  int m_rank;

  virtual void SetUp()
  {
    MPI_Comm_rank(MPI_COMM_WORLD, &m_rank);

    if (shmem_resource == nullptr) {
      auto& rm = umpire::ResourceManager::getInstance();
      auto traits{umpire::get_default_resource_traits("SHARED::POSIX")};
      ASSERT_EQ(traits.scope, umpire::MemoryResourceTraits::shared_scope::node);
      ASSERT_EQ(traits.resource, umpire::MemoryResourceTraits::resource_type::shared);

      traits.size = m_segment_size;
      // NOTE: The name of the allocator MUST have "SHARED::POSIX:: prefix when both IPC and MPI3 enabled.
      ASSERT_NO_THROW(allocator = rm.makeResource(allocator_name, traits););
      auto base_strategy = allocator.getAllocationStrategy();
      shmem_resource = dynamic_cast<umpire::resource::HostSharedMemoryResource*>(base_strategy);
      MPI_Barrier(MPI_COMM_WORLD);
    }

    std::size_t state_size{sizeof(SharedMemoryState) + (m_max_elems * sizeof(std::size_t))};
    ASSERT_NO_THROW(shmem_state = static_cast<SharedMemoryState*>(allocator.allocate(shmem_state_name, state_size)););

    if (m_rank == 0) {
      shmem_state->initial_size = shmem_resource->getActualSize();
      std::cout << "Initial size is: " << shmem_state->initial_size << std::endl;
      shmem_state->largest_allocation_size = find_largest_allocation_size();

      std::random_device rd;
      std::mt19937 gen{rd()};
      std::uniform_int_distribution<std::size_t> distrib(1, m_max_elems);

      for (std::size_t i = 0; i < m_max_allocs; ++i) {
        shmem_state->allocation_sizes[i] = distrib(gen) * sizeof(ArrayElement);
      }

      std::vector<ArrayElement*> allocs{alloc_until_full()};
      shmem_state->num_allocations = allocs.size();
      do_deallocations(allocs);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ASSERT_EQ(shmem_resource->getActualSize(), shmem_state->initial_size);
    MPI_Barrier(MPI_COMM_WORLD);
  }

  virtual void TearDown()
  {
    MPI_Barrier(MPI_COMM_WORLD);
    ASSERT_EQ(shmem_state->initial_size, shmem_resource->getActualSize());

    MPI_Barrier(MPI_COMM_WORLD);

    if (m_rank == 0) {
      ASSERT_EQ(shmem_state->largest_allocation_size, find_largest_allocation_size());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    allocator.deallocate(shmem_state);
    shmem_state = nullptr;

    MPI_Barrier(MPI_COMM_WORLD);
  }

  std::size_t find_largest_allocation_size()
  {
    //
    // The shared memory allocator will use its segment for both the data and
    // supporting metadata (segment header, storage for name, block descriptor
    // linked list, etc).  Consequently, the size used to create the allocator
    // will be too large.
    //
    // This test will continue allocating until it finds the largest size.  As
    // a unit test, this test will then (de)allocate that amount in a loop to
    // insure that there are no problems within the internal accounting in the
    // implementation.
    //
    std::size_t allocation_size{m_segment_size - shmem_state->initial_size};
    void* ptr{nullptr};

    while (allocation_size != 0) {
      try {
        ptr = allocator.allocate(max_allocation_name, allocation_size);
        break;
      } catch (...) {
        allocation_size--;
        continue;
      }
    }

    allocator.deallocate(ptr);

    return allocation_size;
  }

  std::vector<ArrayElement*> alloc_until_full()
  {
    std::vector<ArrayElement*> allocs;

    try {
      for (std::size_t i = 0; i < m_max_allocs; ++i) {
        std::stringstream name;
        name << "size_" << i;
        void* ptr{allocator.allocate(name.str(), shmem_state->allocation_sizes[i])};
        allocs.push_back(static_cast<ArrayElement*>(ptr));
      }
    } catch (...) {
      ;
    }

    return allocs;
  }

  void do_deallocations(std::vector<ArrayElement*>& allocs)
  {
    for (auto& x : allocs) {
      ASSERT_NO_THROW(allocator.deallocate(x););
    }
  }

  std::size_t page_size() const
  {
    long ps = ::sysconf(_SC_PAGESIZE);
    return (ps > 0) ? static_cast<std::size_t>(ps) : 4096;
  }

  void touch_one_byte_per_page(std::uint8_t* buffer, std::size_t bytes)
  {
    const std::size_t ps = page_size();

    for (std::size_t i = 0; i < bytes; i += ps) {
      ++buffer[i];
    }

    if (bytes > 0) {
      ++buffer[bytes - 1];
    }
  }

  std::size_t wait_for_mapping_rss_change(const std::string& mapping_name, std::size_t reference, bool expect_less)
  {
    std::size_t rss = umpire::get_mapping_memory_usage(mapping_name);

    for (int attempt = 0; attempt < 20; ++attempt) {
      if ((expect_less && rss < reference) || (!expect_less && rss > reference)) {
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds{10});
      rss = umpire::get_mapping_memory_usage(mapping_name);
    }

    // Verify that the expected change actually occurred
    if (expect_less) {
      EXPECT_LT(rss, reference) << "RSS did not decrease as expected after 200ms (reference: " << reference
                                << ", final: " << rss << ")";
    } else {
      EXPECT_GT(rss, reference) << "RSS did not increase as expected after 200ms (reference: " << reference
                                << ", final: " << rss << ")";
    }

    return rss;
  }
};

TEST_F(SharedMemoryTest, UnitTests)
{
  MPI_Barrier(MPI_COMM_WORLD);
  {
    std::size_t allocation_size{m_segment_size + 1};
    ASSERT_THROW(allocator.allocate("AllocTooMuch", allocation_size), umpire::runtime_error);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  {
    if (m_rank == 0) {
      for (int loop{0}; loop < 100; loop++) {
        ASSERT_NO_THROW(
            allocator.deallocate(allocator.allocate(max_allocation_name, shmem_state->largest_allocation_size)););
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
  {
    auto allocs = alloc_until_full();
    int size;

    ASSERT_EQ(allocs.size(), m_max_allocs);
    ASSERT_GT(shmem_resource->getActualSize(), shmem_state->initial_size);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    for (std::size_t i{0}; i < allocs.size(); i++) {
      if (int(i % size) != m_rank)
        continue;

      ArrayElement* buffer{allocs[i]};
      std::size_t elems{shmem_state->allocation_sizes[i] / sizeof(ArrayElement)};

      buffer[0] = i + 1;
      buffer[elems - 1] = i + 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    for (std::size_t i{0}; i < allocs.size(); i++) {
      ArrayElement* buffer{allocs[i]};
      std::size_t elems{shmem_state->allocation_sizes[i] / sizeof(ArrayElement)};

      ASSERT_EQ(buffer[0], i + 1);
      ASSERT_EQ(buffer[elems - 1], i + 1);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

    std::shuffle(allocs.begin(), allocs.end(), std::default_random_engine(seed));
    do_deallocations(allocs);

    MPI_Barrier(MPI_COMM_WORLD);
    ASSERT_EQ(shmem_resource->getActualSize(), shmem_state->initial_size);

    MPI_Barrier(MPI_COMM_WORLD);
    ASSERT_NO_THROW(allocator.release(););
    MPI_Barrier(MPI_COMM_WORLD);
    ASSERT_EQ(shmem_resource->getActualSize(), shmem_state->initial_size);

    MPI_Barrier(MPI_COMM_WORLD);
    // `release()` is expected to be safe to call multiple times (idempotent) even
    // when there is nothing left to reclaim from the segment.
    ASSERT_NO_THROW(allocator.release(););
    MPI_Barrier(MPI_COMM_WORLD);
    ASSERT_EQ(shmem_resource->getActualSize(), shmem_state->initial_size);

    MPI_Barrier(MPI_COMM_WORLD);
    if (m_rank == 0) {
      void* ptr = allocator.allocate(max_allocation_name, shmem_state->largest_allocation_size);
      allocator.deallocate(ptr);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
}

TEST_F(SharedMemoryTest, ReleasePreservesActiveAllocations)
{
  constexpr std::size_t live_bytes = 64ULL * 1024ULL;
  constexpr std::size_t free_bytes = 8ULL * 1024ULL * 1024ULL;
  const std::string live_name{"KeepAlive"};
  const std::string free_name{"ReleaseCandidate"};

  auto* live_buffer = static_cast<ArrayElement*>(allocator.allocate(live_name, live_bytes));
  auto* free_buffer = static_cast<std::uint8_t*>(allocator.allocate(free_name, free_bytes));
  const std::size_t live_elems = live_bytes / sizeof(ArrayElement);

  ASSERT_GT(live_elems, static_cast<std::size_t>(2 * std::max(m_rank, 1)));

  const std::size_t low_index = static_cast<std::size_t>(m_rank);
  const std::size_t high_index = live_elems - 1 - static_cast<std::size_t>(m_rank);

  live_buffer[low_index] = 100 + m_rank;
  live_buffer[high_index] = 200 + m_rank;

  MPI_Barrier(MPI_COMM_WORLD);
  allocator.deallocate(free_buffer);
  MPI_Barrier(MPI_COMM_WORLD);

  ASSERT_GT(shmem_resource->getActualSize(), shmem_state->initial_size);

  ASSERT_NO_THROW(allocator.release(););
  MPI_Barrier(MPI_COMM_WORLD);

  ASSERT_EQ(live_buffer[low_index], 100 + m_rank);
  ASSERT_EQ(live_buffer[high_index], 200 + m_rank);

  auto* reused_buffer = static_cast<std::uint8_t*>(allocator.allocate(free_name, free_bytes));
  MPI_Barrier(MPI_COMM_WORLD);

  allocator.deallocate(reused_buffer);
  allocator.deallocate(live_buffer);
  MPI_Barrier(MPI_COMM_WORLD);

  ASSERT_EQ(shmem_resource->getActualSize(), shmem_state->initial_size);
}

TEST_F(SharedMemoryTest, ReleaseReclaimsFreedPages)
{
#if !defined(__linux__)
  GTEST_SKIP() << "requires Linux /proc/self/smaps";
#else
  constexpr std::size_t rss_probe_bytes = 64ULL * 1024ULL * 1024ULL;
  const std::string rss_probe_name{"ReleaseRssProbe"};

  MPI_Barrier(MPI_COMM_WORLD);
  // Only rank 0 performs the RSS measurement to avoid contention on the shared segment.
  // The test verifies that release() correctly returns freed pages to the OS, which can
  // be reliably measured from a single process. Other ranks wait at barriers to ensure
  // synchronization.
  if (m_rank == 0) {
    const std::size_t rss_before = umpire::get_mapping_memory_usage(allocator_name);

    auto* buffer = static_cast<std::uint8_t*>(allocator.allocate(rss_probe_name, rss_probe_bytes));
    touch_one_byte_per_page(buffer, rss_probe_bytes);

    const std::size_t rss_after_touch = wait_for_mapping_rss_change(allocator_name, rss_before, false);
    allocator.deallocate(buffer);

    ASSERT_NO_THROW(allocator.release(););
    const std::size_t rss_after_release = wait_for_mapping_rss_change(allocator_name, rss_after_touch, true);

    ASSERT_GT(rss_after_touch, rss_before);
    ASSERT_LT(rss_after_release, rss_after_touch);
  }
  MPI_Barrier(MPI_COMM_WORLD);

  ASSERT_EQ(shmem_resource->getActualSize(), shmem_state->initial_size);
#endif
}
} // namespace

int main(int argc, char* argv[])
{
  int result = 0;

  ::testing::InitGoogleTest(&argc, argv);

  MPI_Init(&argc, &argv);

  result = RUN_ALL_TESTS();

  MPI_Finalize();

  return result;
}
