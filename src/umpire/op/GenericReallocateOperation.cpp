//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/op/GenericReallocateOperation.hpp"

#include <cstdlib>

#include "umpire/ResourceManager.hpp"
#include "umpire/op.hpp"
#include "umpire/strategy/AllocationStrategy.hpp"
#include "umpire/util/AllocationRecord.hpp"
#include "umpire/util/Macros.hpp"

namespace umpire {
namespace op {

void GenericReallocateOperation::transform(void* current_ptr, void** new_ptr,
                                           util::AllocationRecord* current_allocation,
                                           util::AllocationRecord* new_allocation, std::size_t new_size)
{
  Allocator allocator{new_allocation->strategy};
  *new_ptr = allocator.allocate(new_size);

  const std::size_t old_size = current_allocation->size;
  const std::size_t copy_size = (old_size > new_size) ? new_size : old_size;

  umpire::copy(current_ptr, *new_ptr, copy_size);

  allocator.deallocate(current_ptr);
}

camp::resources::EventProxy<camp::resources::Resource> GenericReallocateOperation::transform_async(
    void* current_ptr, void** new_ptr, util::AllocationRecord* current_allocation,
    util::AllocationRecord* new_allocation, std::size_t new_size, camp::resources::Resource& ctx)
{
  Allocator allocator{new_allocation->strategy};
  *new_ptr = allocator.allocate(new_size);

  const std::size_t old_size = current_allocation->size;
  const std::size_t copy_size = (old_size > new_size) ? new_size : old_size;

  auto event = umpire::copy(current_ptr, *new_ptr, copy_size, ctx);

  allocator.deallocate(current_ptr);

  return event;
}

} // end of namespace op
} // end of namespace umpire
