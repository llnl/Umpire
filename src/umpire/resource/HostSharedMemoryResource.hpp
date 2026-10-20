//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef __Host_Shared_Memory_Resource_HPP
#define __Host_Shared_Memory_Resource_HPP

#include <memory>
#include <string>

#include "umpire/resource/MemoryResource.hpp"
#include "umpire/util/Platform.hpp"

namespace umpire {
namespace resource {

class HostSharedMemoryResource : public MemoryResource {
 public:
  HostSharedMemoryResource(Platform platform, const std::string& name, int id, MemoryResourceTraits traits);

  ~HostSharedMemoryResource();

  void* allocate(std::size_t bytes) override;

  void* allocate_named(const std::string& name, std::size_t bytes) override;

  void deallocate(void* ptr, std::size_t size) override;

  /*!
   * \brief Attempt to return unused shared memory pages to the OS.
   *
   * This resource uses a fixed-size POSIX shared memory segment and maintains
   * free blocks internally. Calling release() will not change allocation
   * semantics, but may allow the OS to reclaim pages that back free blocks so
   * they can be reused by the system.
   */
  void release() override;

  std::size_t getActualSize() const noexcept override;

  bool isAccessibleFrom(Platform p) noexcept override;
  Platform getPlatform() noexcept override;

  void* find_pointer_from_name(const std::string& name);

 protected:
  Platform m_platform;

 private:
  class impl;
  std::unique_ptr<impl> pimpl;

  bool isPageable() noexcept;
};

} // end of namespace resource
} // end of namespace umpire
#endif // __Host_Shared_Memory_Resource_HPP
