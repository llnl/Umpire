//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-24, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef __Host_Mpi3_Shared_Memory_Resource_HPP
#define __Host_Mpi3_Shared_Memory_Resource_HPP

#include <map>
#include <string>

#include "mpi.h"
#include "umpire/resource/MemoryResource.hpp"
#include "umpire/util/Platform.hpp"

namespace umpire {
namespace resource {

/*!
 * \brief Check whether this rank is bound to one socket.
 *
 * This is a local check; it does not communicate with other MPI ranks.
 *
 * \param reason Failure reason when the check returns false.
 *
 * \return true when this rank's CPU affinity maps to one socket.
 */
bool affinity_maps_to_single_socket(std::string& reason);

/*!
 * \brief MemoryResource that exposes MPI-3 shared-memory windows on the host.
 *
 * Allocations created by this resource are backed by an MPI shared window over
 * the communicator selected from the provided traits. The communicator can be
 * scoped to all ranks that share a node or narrowed to ranks that share a
 * socket.
 */
class HostMpi3SharedMemoryResource : public MemoryResource {
 public:
  /*!
   * \brief Construct a host shared-memory resource for an MPI rank group.
   *
   * The resource creates an internal communicator from the process-global MPI
   * communicator using \p traits.scope. Allocations made through this resource
   * are only visible to ranks that belong to that derived communicator.
   *
   * \param name Name used to register the resource with Umpire.
   * \param id Unique identifier assigned to this resource instance.
   * \param traits Resource traits for the shared allocation. For this resource,
   *        \p traits.scope selects whether the shared communicator is built at
   *        node scope or socket scope.
   */
  HostMpi3SharedMemoryResource(const std::string& name, int id, MemoryResourceTraits traits);

  ~HostMpi3SharedMemoryResource();

  void* allocate(std::size_t bytes) override;

  void deallocate(void* ptr, std::size_t size) override;

  bool isAccessibleFrom(Platform p) noexcept override;

  Platform getPlatform() noexcept override;

  /*!
   * \brief Return the communicator used for this resource's shared windows.
   *
   * This is the communicator passed to MPI shared-memory allocation routines,
   * so it contains exactly the ranks that can directly attach to allocations
   * from this resource.
   *
   * \return MPI communicator that defines the sharing domain for allocations.
   */
  MPI_Comm getSharedCommunicator() const noexcept;

 private:
  static int free_comm(MPI_Comm comm, int keyval, void* attribute_val, void* extra_state);

  MPI_Comm m_shared_comm;
  int m_local_rank;
  std::map<void*, MPI_Win> m_shared_windows;
};

} // end of namespace resource
} // end of namespace umpire
#endif // __Host_Mpi3_Shared_Memory_Resource_HPP
