//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-24, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/resource/HostMpi3SharedMemoryResourceFactory.hpp"

#include "umpire/resource/HostMpi3SharedMemoryResource.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"
#include "umpire/util/make_unique.hpp"
#include "umpire/util/shared_memory_helper.hpp"

namespace umpire {
namespace resource {

bool HostMpi3SharedMemoryResourceFactory::isValidMemoryResourceFor(const std::string& name) noexcept
{
  return umpire::util::matchesSharedMemoryResource(name, "MPI3");
}

std::unique_ptr<resource::MemoryResource> HostMpi3SharedMemoryResourceFactory::create(const std::string& name, int id)
{
  return create(name, id, getDefaultTraits());
}

std::unique_ptr<resource::MemoryResource> HostMpi3SharedMemoryResourceFactory::create(const std::string& name, int id,
                                                                                      MemoryResourceTraits traits)
{
  if (traits.scope != MemoryResourceTraits::shared_scope::node &&
      traits.scope != MemoryResourceTraits::shared_scope::socket) {
    UMPIRE_ERROR(runtime_error, "HostMpi3SharedMemoryResource only supports shared_scope::node or ::socket");
  }
  return util::make_unique<HostMpi3SharedMemoryResource>(name, id, traits);
}

MemoryResourceTraits HostMpi3SharedMemoryResourceFactory::getDefaultTraits()
{
  MemoryResourceTraits traits;

  traits.unified = false;
  traits.size = 0;

  traits.vendor = MemoryResourceTraits::vendor_type::unknown;
  traits.kind = MemoryResourceTraits::memory_type::unknown;
  traits.used_for = MemoryResourceTraits::optimized_for::any;
  traits.resource = MemoryResourceTraits::resource_type::shared;
  traits.size = 16 * 1024 * 1024;
  traits.scope = MemoryResourceTraits::shared_scope::node;

  return traits;
}

} // end of namespace resource
} // end of namespace umpire
