//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_FixedMallocPool_HPP
#define UMPIRE_FixedMallocPool_HPP

#include <cstdlib>
#include <vector>

namespace umpire {
namespace util {

/*!
 * \brief Pool for fixed size allocations using malloc()
 *
 * Another version of this class exists in umpire::strategy, but this
 * version does not rely on Allocator and all the memory tracking
 * statistics, so it is useful for building objects in umpire::util.
 */
class FixedMallocPool {
 public:
  FixedMallocPool() = delete;

  FixedMallocPool(const std::size_t object_bytes, const std::size_t objects_per_pool = 1024 * 1024);

  // Copy construction is not allowed. Doing so would create two
  // FixedMallocPool objects with m_pool that point to the same allocated
  // data; this data would be freed when one FixedMallocPool object is
  // destroyed, leaving the other with a dangling pointer.
  FixedMallocPool(const FixedMallocPool& other) = delete;

  // Copy assignment is not allowed (same reasoning as copy construction).
  FixedMallocPool& operator=(const FixedMallocPool& other) = delete;

  ~FixedMallocPool();

  void* allocate(std::size_t bytes = 0);
  void deallocate(void* ptr);

  std::size_t numPools() const noexcept;
  std::size_t totalBytes() const noexcept;

 private:
  struct Pool {
    unsigned char* data;
    unsigned int free_list;  // index of first recycled slot; linked through the
                             // slots' own storage, objects_per_pool = end of list
    unsigned int num_used;   // high-water mark: slots ever handed out
    Pool(const std::size_t object_bytes, const std::size_t objects_per_pool);
  };

  void newPool();
  void* allocInPool(Pool& p) noexcept;

  // allocate_impl used to avoid repeated ASSERT in allocate
  void* allocate_impl(std::size_t bytes);

  unsigned char* addr_from_index(const Pool& p, unsigned int i) const;
  unsigned int index_from_addr(const Pool& p, const unsigned char* ptr) const;

  const std::size_t m_obj_bytes;
  const std::size_t m_obj_per_pool;
  const std::size_t m_data_bytes;
  std::vector<Pool> m_pool;
  // NOTE: struct Pool lacks a non-trivial destructor. If m_pool is
  // ever reduced in size, then .data has to be manually deallocated
  // to avoid a memory leak.
};

} // namespace util
} // end namespace umpire

#endif // UMPIRE_FixedPool_HPP
