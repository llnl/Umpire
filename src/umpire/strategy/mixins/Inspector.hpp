//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_Inspector_HPP
#define UMPIRE_Inspector_HPP

#include "umpire/config.hpp"
#include "umpire/util/AllocationRecord.hpp"

#include <memory>
#include <string>

namespace umpire {
namespace strategy {

class AllocationStrategy;

namespace mixins {

class Inspector
{
  public:
    Inspector() = default;

    void registerAllocation(void* ptr, std::size_t size, strategy::AllocationStrategy* strategy);

    void registerAllocation(void* ptr, std::size_t size, strategy::AllocationStrategy* strategy, const std::string& name);

    // Deregisters the allocation if the strategy matches, otherwise throws an error
    util::AllocationRecord deregisterAllocation(void* ptr, strategy::AllocationStrategy* strategy);

#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
    // Zero-byte allocations carry no header, so only the strategy counters are updated
    void deregisterNullAllocation(strategy::AllocationStrategy* strategy);
#endif
};

} // end of namespace mixins
} // end of namespace strategy
} // end of namespace umpire

#endif // UMPIRE_Inspector_HPP
