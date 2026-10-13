//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_AllocationHeader_HPP
#define UMPIRE_AllocationHeader_HPP

#include <cstddef>
#include <cstdint>

namespace umpire {

namespace strategy {
class AllocationStrategy;
}

namespace util {

/*!
 * \brief Minimal introspection metadata stored directly in front of each
 * allocation when Umpire is built with UMPIRE_ENABLE_INTROSPECTION_HEADER.
 *
 * The header replaces the global AllocationMap: allocations are padded by
 * allocation_header_size bytes, the header is written at the base of the
 * padded block, and the user receives the pointer immediately after it.
 * The header can then be read back in constant time from any base pointer.
 *
 * This scheme requires that all memory is readable from the host.
 */
struct AllocationHeader {
  void* ptr;
  std::size_t size;
  strategy::AllocationStrategy* strategy;
};

/*!
 * \brief Alignment preserved for user pointers when the header is prepended:
 * at least max_align_t, and no less than the 16-byte default alignment
 * guaranteed by Umpire's pool strategies.
 */
constexpr std::size_t allocation_header_alignment{alignof(std::max_align_t) > 16 ? alignof(std::max_align_t) : 16};

/*!
 * \brief Size reserved in front of each allocation for the AllocationHeader,
 * padded so that user pointers keep allocation_header_alignment.
 */
constexpr std::size_t allocation_header_size{
    ((sizeof(AllocationHeader) + allocation_header_alignment - 1) / allocation_header_alignment) *
    allocation_header_alignment};

/*!
 * \brief Write an AllocationHeader at base_ptr, returning the user pointer
 * located directly after the header.
 */
inline void* write_allocation_header(void* base_ptr, std::size_t size, strategy::AllocationStrategy* strategy) noexcept
{
  void* user_ptr{reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base_ptr) + allocation_header_size)};

  AllocationHeader* header{static_cast<AllocationHeader*>(base_ptr)};
  header->ptr = user_ptr;
  header->size = size;
  header->strategy = strategy;

  return user_ptr;
}

/*!
 * \brief Read the AllocationHeader for the allocation with the given user
 * pointer.
 */
inline AllocationHeader* get_allocation_header(void* user_ptr) noexcept
{
  return reinterpret_cast<AllocationHeader*>(reinterpret_cast<uintptr_t>(user_ptr) - allocation_header_size);
}

/*!
 * \brief Recover the base pointer of the padded block for the allocation with
 * the given user pointer.
 */
inline void* get_base_pointer(void* user_ptr) noexcept
{
  return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(user_ptr) - allocation_header_size);
}

} // end of namespace util
} // end of namespace umpire

#endif // UMPIRE_AllocationHeader_HPP
