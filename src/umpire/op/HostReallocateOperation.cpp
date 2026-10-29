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
#include "umpire/util/error.hpp"
#include "umpire/op.hpp"

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

    umpire::copy(current_ptr, *new_ptr, copy_size);
    allocator.deallocate(current_ptr);
  } else {
    auto old_record = ResourceManager::getInstance().deregisterAllocation(current_ptr);
    *new_ptr = ::realloc(current_ptr, new_size);

    if (!*new_ptr) {
      UMPIRE_ERROR(runtime_error, fmt::format("::realloc(current_ptr={}, old_size={}, new_size={}) failed.",
                                              current_ptr, old_record.size, new_size));
    }

    ResourceManager::getInstance().registerAllocation(*new_ptr, {*new_ptr, new_size, new_allocation->strategy});
  }
}

} // end of namespace op
} // end of namespace umpire
