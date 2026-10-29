//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/op/HostReallocateOperation.hpp"

#include <cstdlib>

#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/mixins/Inspector.hpp"
#include "umpire/util/AllocationHeader.hpp"
#include "umpire/util/error.hpp"

namespace umpire {
namespace op {

void HostReallocateOperation::transform(void* current_ptr, void** new_ptr, util::AllocationRecord* current_allocation,
                                        util::AllocationRecord* new_allocation, std::size_t new_size)
{
  auto allocator = umpire::Allocator(new_allocation->strategy);
  const std::size_t old_size = current_allocation->size;

  //
  // Since Umpire implements its own semantics for zero-length allocations, we
  // cannot simply call ::realloc() with a pointer to a zero-length allocation.
  //
  if (old_size == 0) {
    *new_ptr = allocator.allocate(new_size);
    const std::size_t copy_size = (old_size > new_size) ? new_size : old_size;

    ResourceManager::getInstance().copy(*new_ptr, current_ptr, copy_size);
    allocator.deallocate(current_ptr);
  } else {
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
    void* base_ptr{::realloc(util::get_base_pointer(current_ptr), new_size + util::allocation_header_size)};

    if (!base_ptr) {
      UMPIRE_ERROR(runtime_error, fmt::format("::realloc(current_ptr={}, old_size={}, new_size={}) failed.",
                                              current_ptr, old_size, new_size));
    }

    *new_ptr = util::write_allocation_header(base_ptr, new_size, new_allocation->strategy);
#else
    auto old_record = ResourceManager::getInstance().deregisterAllocation(current_ptr);
    *new_ptr = ::realloc(current_ptr, new_size);

    if (!*new_ptr) {
      UMPIRE_ERROR(runtime_error, fmt::format("::realloc(current_ptr={}, old_size={}, new_size={}) failed.",
                                              current_ptr, old_record.size, new_size));
    }

    ResourceManager::getInstance().registerAllocation(*new_ptr, {*new_ptr, new_size, new_allocation->strategy});
#endif
  }
}

} // end of namespace op
} // end of namespace umpire
