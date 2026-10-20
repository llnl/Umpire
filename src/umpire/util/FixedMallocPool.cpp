//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

// FixedMallocPool hands out fixed-size slots from malloc()'d pools.
// Recycled slots are kept on a LIFO free list of slot indices linked
// through the slots' own storage; slots that have never been handed out
// are dispensed in order with a bump pointer and are never read or
// written until allocated.

#include "umpire/util/FixedMallocPool.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"

namespace umpire {
namespace util {

inline unsigned char* FixedMallocPool::addr_from_index(const FixedMallocPool::Pool& p, unsigned int i) const
{
  return p.data + i * m_obj_bytes;
}

inline unsigned int FixedMallocPool::index_from_addr(const FixedMallocPool::Pool& p, const unsigned char* ptr) const
{
  return static_cast<unsigned int>((ptr - p.data) / m_obj_bytes);
}

FixedMallocPool::Pool::Pool(const std::size_t object_bytes, const std::size_t objects_per_pool)
    : data(static_cast<unsigned char*>(std::malloc(object_bytes * objects_per_pool))),
      free_list(static_cast<unsigned int>(objects_per_pool)),
      num_used(0)
{
}

FixedMallocPool::FixedMallocPool(const std::size_t object_bytes, const std::size_t objects_per_pool)
    : m_obj_bytes(object_bytes), m_obj_per_pool(objects_per_pool), m_data_bytes(m_obj_bytes * m_obj_per_pool), m_pool()
{
  // Free-list links are stored in the slots themselves
  UMPIRE_ASSERT(object_bytes >= sizeof(unsigned int));
  // Slot indices (free-list links and num_used) are unsigned int
  UMPIRE_ASSERT(objects_per_pool <= static_cast<std::size_t>(std::numeric_limits<unsigned int>::max()));
  newPool();
}

FixedMallocPool::~FixedMallocPool()
{
  for (auto& a : m_pool)
    std::free(a.data);
}

void FixedMallocPool::newPool()
{
  m_pool.emplace_back(m_obj_bytes, m_obj_per_pool);
}

void* FixedMallocPool::allocInPool(Pool& p) noexcept
{
  if (p.free_list < m_obj_per_pool) {
    unsigned char* ret = addr_from_index(p, p.free_list);
    p.free_list = *reinterpret_cast<unsigned int*>(ret);
    return static_cast<void*>(ret);
  }

  if (p.num_used < m_obj_per_pool) {
    return static_cast<void*>(addr_from_index(p, p.num_used++));
  }

  return nullptr;
}

void* FixedMallocPool::allocate_impl(std::size_t bytes)
{
  void* ptr = nullptr;

  for (auto it = m_pool.rbegin(); it != m_pool.rend(); ++it) {
    ptr = allocInPool(*it);
    if (ptr)
      return ptr;
  }

  if (!ptr) {
    newPool();
    ptr = allocate_impl(bytes);
  }

  // Could be an error, but FixedMallocPool is used internally and an
  // error would be unrecoverable
  UMPIRE_ASSERT(ptr);

  return ptr;
}

void* FixedMallocPool::allocate(std::size_t bytes)
{
  UMPIRE_ASSERT(bytes <= m_obj_bytes);
  return allocate_impl(bytes);
}

void FixedMallocPool::deallocate(void* ptr)
{
  for (auto& p : m_pool) {
    const unsigned char* t_ptr = reinterpret_cast<unsigned char*>(ptr);
    const ptrdiff_t offset = t_ptr - p.data;
    if ((offset >= 0) && (offset < static_cast<ptrdiff_t>(m_data_bytes))) {
      *reinterpret_cast<unsigned int*>(ptr) = p.free_list;
      p.free_list = index_from_addr(p, t_ptr);
      return;
    }
  }

  UMPIRE_ERROR(runtime_error, "Could not find the pointer to deallocate");
}

std::size_t FixedMallocPool::numPools() const noexcept
{
  return m_pool.size();
}

std::size_t FixedMallocPool::totalBytes() const noexcept
{
  return numPools() * m_data_bytes;
}

} // namespace util
} // end of namespace umpire
