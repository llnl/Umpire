//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-24, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "umpire/resource/HostMpi3SharedMemoryResource.hpp"

#include <string>

#include "umpire/resource/MemoryResource.hpp"
#include "umpire/util/MPI.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/affinity.hpp"
#include "umpire/util/error.hpp"

namespace umpire {
namespace resource {

namespace {

constexpr int IGNORE_KEY{0};

/*!
 * \brief Build the communicator that defines which ranks share allocations.
 *
 * \param comm Parent communicator used to discover ranks that may share host
 *        memory.
 * \param scope Requested sharing scope from MemoryResourceTraits. Node scope
 *        keeps one communicator per node, while socket scope further splits the
 *        node-local communicator using socket affinity.
 *
 * \return Communicator containing exactly the ranks that participate in each
 * shared-memory allocation for the resource.
 */
MPI_Comm create_shared_communicator(MPI_Comm comm, MemoryResourceTraits::shared_scope scope)
{
  MPI_Comm shared_comm{MPI_COMM_NULL};

  if (scope == MemoryResourceTraits::shared_scope::node) {
    util::check_mpi_call(MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, IGNORE_KEY, MPI_INFO_NULL, &shared_comm),
                         "MPI_Comm_split_type");
    return shared_comm;
  }

  if (scope == MemoryResourceTraits::shared_scope::socket) {
    MPI_Comm node_comm{MPI_COMM_NULL};
    util::check_mpi_call(MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, IGNORE_KEY, MPI_INFO_NULL, &node_comm),
                         "MPI_Comm_split_type");

    int color{-1};
    std::string reason;
    const int local_affinity_valid = util::get_socket_id_from_affinity(color, reason) ? 1 : 0;
    int all_affinity_valid{0};

    const int allreduce_status = MPI_Allreduce(&local_affinity_valid, &all_affinity_valid, 1, MPI_INT, MPI_MIN, comm);
    if (allreduce_status != MPI_SUCCESS) {
      MPI_Comm_free(&node_comm);
      util::check_mpi_call(allreduce_status, "MPI_Allreduce");
    }

    if (!all_affinity_valid) {
      util::check_mpi_call(MPI_Comm_free(&node_comm), "MPI_Comm_free");
      if (local_affinity_valid) {
        reason = "Another rank in the parent communicator could not determine a single socket affinity";
      }
      UMPIRE_ERROR(runtime_error, reason);
    }

    UMPIRE_LOG(Debug, "Creating socket-scoped shared communicator with color " << color);

    const int split_status = MPI_Comm_split(node_comm, color, IGNORE_KEY, &shared_comm);
    if (split_status != MPI_SUCCESS) {
      if (node_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&node_comm);
      }
      util::check_mpi_call(split_status, "MPI_Comm_split");
    }

    util::check_mpi_call(MPI_Comm_free(&node_comm), "MPI_Comm_free");

    return shared_comm;
  }

  UMPIRE_ERROR(runtime_error,
               fmt::format("Unsupported shared communicator scope: {}", to_string(scope)));
}

} // end anonymous namespace

bool affinity_maps_to_single_socket(std::string& reason)
{
  return util::affinity_maps_to_single_socket(reason);
}

HostMpi3SharedMemoryResource::HostMpi3SharedMemoryResource(const std::string& name, int id, MemoryResourceTraits traits)
    : MemoryResource{name, id, traits}
{
  m_shared_comm = create_shared_communicator(util::MPI::getCommunicator(), traits.scope);
  util::check_mpi_call(MPI_Comm_rank(m_shared_comm, &m_local_rank), "MPI_Comm_rank");

  // Free the comm at exit during cleanup in MPI_Finalize. We pass the m_shared_comm
  // by turning it into an int (as for Fortran) and then decoding that in the callback.
  int keyval = 0;
  util::check_mpi_call(MPI_Comm_create_keyval(MPI_COMM_NULL_COPY_FN, free_comm, &keyval, nullptr),
                       "MPI_Comm_create_keyval");
  util::check_mpi_call(MPI_Comm_set_attr(MPI_COMM_SELF, keyval, (void*)(intptr_t)MPI_Comm_c2f(m_shared_comm)),
                       "MPI_Comm_set_attr");
}

HostMpi3SharedMemoryResource::~HostMpi3SharedMemoryResource()
{
  // NOTE: m_shared_comm is freed at cleanup pre MPI_Finalize
}

void* HostMpi3SharedMemoryResource::allocate(std::size_t bytes)
{
  void* ptr{nullptr};
  MPI_Win win{MPI_WIN_NULL};
  MPI_Aint local_size = (m_local_rank != 0) ? 0 : bytes;
  MPI_Aint size = bytes;
  int disp{sizeof(unsigned char)};

  util::check_mpi_call(MPI_Win_allocate_shared(local_size, disp, MPI_INFO_NULL, m_shared_comm, &ptr, &win),
                       "MPI_Win_allocate_shared");
  util::check_mpi_call(MPI_Win_shared_query(win, 0, &size, &disp, &ptr), "MPI_Win_shared_query");
  m_shared_windows[ptr] = win;

  return ptr;
}

void HostMpi3SharedMemoryResource::deallocate(void* ptr, std::size_t)
{
  auto window = m_shared_windows.find(ptr);
  if (window != m_shared_windows.end()) {
    util::check_mpi_call(MPI_Win_free(&(window->second)), "MPI_Win_free");
    m_shared_windows.erase(window);
  } else {
    UMPIRE_ERROR(umpire::unknown_pointer_error, "");
  }
}

bool HostMpi3SharedMemoryResource::isAccessibleFrom(Platform p) noexcept
{
  if (p == Platform::host)
    return true;
  else
    return false;
}

Platform HostMpi3SharedMemoryResource::getPlatform() noexcept
{
  return Platform::host;
}

MPI_Comm HostMpi3SharedMemoryResource::getSharedCommunicator() const noexcept
{
  return m_shared_comm;
}

int HostMpi3SharedMemoryResource::free_comm(MPI_Comm UMPIRE_UNUSED_ARG(comm), int UMPIRE_UNUSED_ARG(keyval),
                                            void* attribute_val, void* UMPIRE_UNUSED_ARG(extra_state))
{
  // Interpret attribute_val as a MPI_Fint comm number.
  const auto comm_number = (MPI_Fint)(intptr_t)(attribute_val);
  MPI_Comm comm_to_free = MPI_Comm_f2c(comm_number);
  return MPI_Comm_free(&comm_to_free);
}

} // end of namespace resource
} // end of namespace umpire
