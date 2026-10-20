#include <mpi.h>

#include <iostream>
#include <string>

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"
#include "umpire/util/MemoryResourceTraits.hpp"

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  auto& rm = umpire::ResourceManager::getInstance();

  int world_rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  // Use MPI3 shared memory resource
  // Note: Could also use "SHARED"
  auto traits = umpire::get_default_resource_traits("SHARED::MPI3");
  traits.size = 1 * 1024 * 1024; // 1 MB

  // Node scope is the default for MPI3 shared memory; socket scope is also supported.
  // Pass `--socket` to request socket scope (falls back to node scope when the rank is not pinned to one socket).
  const bool request_socket_scope = (argc > 1) && (std::string{argv[1]} == "--socket");
  traits.scope = request_socket_scope ? umpire::MemoryResourceTraits::shared_scope::socket
                                      : umpire::MemoryResourceTraits::shared_scope::node;

  if (traits.scope == umpire::MemoryResourceTraits::shared_scope::socket) {
    std::string reason;
    if (!umpire::can_use_socket_scoped_mpi3_shared_memory(MPI_COMM_WORLD, reason)) {
      if (world_rank == 0) {
        std::cerr << "Requested socket-scoped MPI3 shared memory, but CPU affinity does not map to a single socket: "
                  << reason << "\nFalling back to node-scoped shared memory.\n";
      }
      traits.scope = umpire::MemoryResourceTraits::shared_scope::node;
    } else {
      if (world_rank == 0) {
        std::cout << "Running with socket-scope!" << std::endl;
      }
    }
  }

  // Create allocator using MPI3 shared memory
  auto mpi3_shm_allocator = rm.makeResource("SHARED::MPI3::mpi3_alloc", traits);

  // Get communicator for the allocator
  MPI_Comm shm_comm = umpire::get_communicator_for_allocator(mpi3_shm_allocator, MPI_COMM_WORLD);

  int rank = 0;
  MPI_Comm_rank(shm_comm, &rank);

  // Allocate shared memory, doesn't need a name for allocation
  uint64_t* data = static_cast<uint64_t*>(mpi3_shm_allocator.allocate(sizeof(uint64_t)));

  if (rank == 0) {
    *data = 0xCAFEBABE;
  }

  MPI_Barrier(shm_comm);

  // All ranks should see the same value
  std::cout << "Rank " << rank << " sees value: " << std::hex << *data << std::endl;

  mpi3_shm_allocator.deallocate(data);

  // Since we called get_communicator_for_allocator(), clean up is needed
  umpire::cleanup_cached_communicators();
  MPI_Finalize();

  return 0;
}
