//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_aligned_allocation_INL
#define UMPIRE_aligned_allocation_INL

#include <limits>

#include "umpire/util/allocation_statistics.hpp"
#include "umpire/util/error.hpp"
#include "umpire/util/memory_sanitizers.hpp"
#include "umpire/util/Macros.hpp"

namespace umpire {
namespace strategy {
namespace mixins {

inline std::size_t AlignedAllocation::aligned_round_up(std::size_t size)
{
  const std::size_t padding{(m_alignment - 1) - (size - 1) % m_alignment};

  if (size > std::numeric_limits<std::size_t>::max() - padding) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot round {} bytes up to alignment {}, the result does not fit in "
                                            "std::size_t",
                                            size, m_alignment));
  }

  return size + padding;
}

inline void* AlignedAllocation::aligned_allocate(std::size_t size)
{
  if (size > std::numeric_limits<std::size_t>::max() - m_alignment) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot allocate {} bytes with alignment {}, the total does not fit in "
                                            "std::size_t",
                                            size, m_alignment));
  }

  std::size_t total_bytes{ size + m_alignment };
  uintptr_t ptr{ reinterpret_cast<uintptr_t>(m_allocator->allocate_internal(total_bytes)) };

  UMPIRE_POISON_MEMORY_REGION(m_allocator, reinterpret_cast<void*>(ptr), total_bytes);

  uintptr_t alignment{ m_alignment - 1 };
  uintptr_t mask{ m_mask };
  void *aligned_ptr{ reinterpret_cast<void*>( (ptr + alignment) & mask ) };

  base_pointer_map[aligned_ptr] = std::make_tuple(
    reinterpret_cast<void*>(ptr), total_bytes);

  return aligned_ptr;
}

inline void AlignedAllocation::aligned_deallocate(void* ptr)
{
  auto ptr_info = base_pointer_map[ptr];
  void* buffer{ std::get<0>(ptr_info) };
  std::size_t size = std::get<1>(ptr_info);
  base_pointer_map.erase(ptr);
  m_allocator->deallocate_internal(buffer, size);
}

} // namespace mixins
} // namespace strategy
} // namespace umpire
#endif // UMPIRE_aligned_allocation_INL
