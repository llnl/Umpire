//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/strategy/DeviceIpcAllocator.hpp"
#include "umpire/util/MemoryResourceTraits.hpp"
#include "umpire/op.hpp"
#if defined(UMPIRE_ENABLE_MPI)
#include <mpi.h>
#endif
#if defined(UMPIRE_ENABLE_CUDA)
#include <cuda_runtime_api.h>
#elif defined(UMPIRE_ENABLE_HIP)
#include <hip/hip_runtime_api.h>
#endif

// Example of using the DeviceIpcAllocator
int main(int argc, char** argv)
{
#if defined(UMPIRE_ENABLE_MPI)
  MPI_Init(&argc, &argv);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#else
  int rank = 0;
#endif

  {
    auto& rm = umpire::ResourceManager::getInstance();

    // Create the IPC allocator with node scope and 1MB shared memory
    auto ipc_allocator = rm.makeAllocator<umpire::strategy::DeviceIpcAllocator>("device_ipc_allocator");
    auto host_allocator = rm.getAllocator("HOST");

    // Allocate device memory - only rank 0 will physically allocate
    // All other ranks will import via IPC
    constexpr std::size_t num_elements = 1024;
    const size_t size = num_elements * sizeof(float);
    float* data = static_cast<float*>(ipc_allocator.allocate(size));

    std::cout << "Rank " << rank << ": Got device memory at " << data << std::endl;

    auto scope_comm = umpire::get_communicator_for_allocator(ipc_allocator, MPI_COMM_WORLD);
    int scope_rank;
    MPI_Comm_rank(scope_comm, &scope_rank);

    // Initialize data from rank 0
    if (scope_rank == 0) {
      // Initialize on host (simplified example)
      float* host_data = static_cast<float*>(host_allocator.allocate(size));
      for (int i = 0; i < 1024; i++) {
        host_data[i] = static_cast<float>(rank);
      }

      // Copy to device
      umpire::copy(host_data, data, num_elements);
      host_allocator.deallocate(host_data);
    }

#if defined(UMPIRE_ENABLE_MPI)
    MPI_Barrier(scope_comm);
#endif

#if defined(UMPIRE_ENABLE_CUDA)
    cudaDeviceSynchronize();
#elif defined(UMPIRE_ENABLE_HIP)
    hipDeviceSynchronize();
#endif

    // All ranks can now access the data
    // Verify by copying a portion back to host
    float* value = static_cast<float*>(host_allocator.allocate(sizeof(float)));
    umpire::copy(data + 1, value, 1);

    std::cout << "Rank " << rank << ": second value is " << *value << std::endl;

    // Deallocate memory on all ranks
    ipc_allocator.deallocate(data);
    host_allocator.deallocate(value);
  }

#if defined(UMPIRE_ENABLE_MPI)
  MPI_Finalize();
#endif

  return 0;
}
