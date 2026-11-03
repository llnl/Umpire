//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/strategy/mixins/Inspector.hpp"

#include "umpire/ResourceManager.hpp"
#include "umpire/util/AllocationHeader.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"

#include <string>

namespace umpire {
namespace strategy {
namespace mixins {

#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)

void Inspector::registerAllocation(void* UMPIRE_UNUSED_ARG(ptr), std::size_t size, strategy::AllocationStrategy* s)
{
  s->m_current_size += size;
  s->m_allocation_count++;

  if (s->m_current_size > s->m_high_watermark) {
    s->m_high_watermark = s->m_current_size;
  }
}

void Inspector::registerAllocation(void* UMPIRE_UNUSED_ARG(ptr), std::size_t size, strategy::AllocationStrategy* s,
                                   const std::string& UMPIRE_UNUSED_ARG(name))
{
  // Allocation names are not retained with header-based introspection
  s->m_current_size += size;
  s->m_allocation_count++;

  if (s->m_current_size > s->m_high_watermark) {
    s->m_high_watermark = s->m_current_size;
  }
}

util::AllocationRecord
Inspector::deregisterAllocation(void* ptr, strategy::AllocationStrategy* s)
{
  auto header = util::get_allocation_header(ptr);

  if (header->ptr != ptr) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot find allocation header for {}", ptr));
  }

  if (header->strategy != s) {
    UMPIRE_ERROR(runtime_error, fmt::format("{} was not allocated by {}", ptr, s->getName()));
  }

  s->m_current_size -= header->size;
  s->m_allocation_count--;

  return {ptr, header->size, s};
}

void Inspector::deregisterNullAllocation(strategy::AllocationStrategy* s)
{
  // Zero-byte allocations carry no header, only the counters are updated
  s->m_allocation_count--;
}

#else

void Inspector::registerAllocation(void* ptr, std::size_t size, strategy::AllocationStrategy* s)
{
  s->m_current_size += size;
  s->m_allocation_count++;

  if (s->m_current_size > s->m_high_watermark) {
    s->m_high_watermark = s->m_current_size;
  }

  ResourceManager::getInstance().registerAllocation(ptr, {ptr, size, s});
}

void Inspector::registerAllocation(void* ptr, std::size_t size, strategy::AllocationStrategy* s, const std::string& name)
{
  s->m_current_size += size;
  s->m_allocation_count++;

  if (s->m_current_size > s->m_high_watermark) {
    s->m_high_watermark = s->m_current_size;
  }

  ResourceManager::getInstance().registerAllocation(ptr, {ptr, size, s, name});
}

util::AllocationRecord
Inspector::deregisterAllocation(void* ptr, strategy::AllocationStrategy* s)
{
  auto record = ResourceManager::getInstance().deregisterAllocation(ptr);

  if (record.strategy == s) {
    s->m_current_size -= record.size;
    s->m_allocation_count--;
  } else {
    // Re-register the pointer and throw an error
    ResourceManager::getInstance().registerAllocation(ptr, {ptr, record.size, record.strategy, record.name});
    UMPIRE_ERROR(runtime_error, fmt::format("{} was not allocated by {}", ptr, s->getName()));
  }

  return record;
}

#endif // UMPIRE_ENABLE_INTROSPECTION_HEADER

} // end of namespace mixins
} // end of namespace strategy
} // end of namespace umpire
