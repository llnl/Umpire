//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <fstream>
#include <iostream>

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/strategy/DeviceIpcAllocator.hpp"
#include "umpire/util/MemoryResourceTraits.hpp"

// This recipe requires MPI: the DeviceIpcAllocator coordinates the IPC
// handles between ranks with MPI, and is only built when UMPIRE_ENABLE_MPI
// and UMPIRE_ENABLE_IPC_SHARED_MEMORY are enabled.
#include <mpi.h>
#if defined(UMPIRE_ENABLE_CUDA)
#include <cuda_runtime_api.h>
#elif defined(UMPIRE_ENABLE_HIP)
#include <hip/hip_runtime_api.h>
#endif

// Example of reading a tetrahedral mesh from a file into GPU memory that is
// shared (via IPC) by every rank on the same socket.
//
// The mesh file is a simple text format:
//   num_nodes num_tets
//   x y z        (one line per node)
//   a b c d      (one line per tetrahedron, node indices)
//
// The final structure is three arrays of doubles (node coordinates) and four
// arrays of integers (tetrahedron connectivity). Within each socket, only the
// scope-leader rank reads the file and copies the data to the GPU; all other
// ranks import the same physical GPU memory through IPC handles.

namespace {

// Write a small example mesh (two tetrahedra sharing a face) so the recipe
// can run out of the box. In a real application the file already exists.
void write_sample_mesh(const std::string& filename)
{
  std::ofstream out(filename);
  out << "5 2\n";
  out << "0.0 0.0 0.0\n";
  out << "1.0 0.0 0.0\n";
  out << "0.0 1.0 0.0\n";
  out << "0.0 0.0 1.0\n";
  out << "1.0 1.0 1.0\n";
  out << "0 1 2 3\n";
  out << "1 2 3 4\n";
}

} // namespace

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  // Stand-in for the application's communicator. The DeviceIpcAllocator
  // itself performs its socket split collectively over MPI_COMM_WORLD, so
  // every rank in MPI_COMM_WORLD must construct the allocator and call
  // allocate/deallocate on it, but all file-related coordination below only
  // uses app_comm.
  MPI_Comm app_comm;
  MPI_Comm_dup(MPI_COMM_WORLD, &app_comm);

  int app_rank;
  MPI_Comm_rank(app_comm, &app_rank);

  const std::string filename = (argc > 1) ? argv[1] : "tet_mesh.txt";

  if (app_rank == 0) {
    std::ifstream probe(filename);
    if (!probe.good()) {
      write_sample_mesh(filename);
    }
  }
  MPI_Barrier(app_comm);

  {
    auto& rm = umpire::ResourceManager::getInstance();
    auto host_allocator = rm.getAllocator("HOST");

    // Create an IPC allocator with socket scope: ranks whose current GPU has
    // the same PCI address share each allocation.
    auto ipc_allocator = rm.makeAllocator<umpire::strategy::DeviceIpcAllocator>(
        "mesh_ipc_allocator", rm.getAllocator("DEVICE"), umpire::MemoryResourceTraits::shared_scope::socket);

    // Every rank needs the mesh sizes to request identically-sized
    // allocations, so rank 0 reads the header and broadcasts it.
    long header[2] = {0, 0};
    if (app_rank == 0) {
      std::ifstream in(filename);
      in >> header[0] >> header[1];
    }
    MPI_Bcast(header, 2, MPI_LONG, 0, app_comm);

    const std::size_t num_nodes = static_cast<std::size_t>(header[0]);
    const std::size_t num_tets = static_cast<std::size_t>(header[1]);
    const std::size_t coord_bytes = num_nodes * sizeof(double);
    const std::size_t conn_bytes = num_tets * sizeof(int);

    // Collective within each socket: the scope leader physically allocates,
    // everyone else imports. The calls must happen in the same order with the
    // same sizes on every rank in the scope.
    double* x = static_cast<double*>(ipc_allocator.allocate(coord_bytes));
    double* y = static_cast<double*>(ipc_allocator.allocate(coord_bytes));
    double* z = static_cast<double*>(ipc_allocator.allocate(coord_bytes));
    int* conn_a = static_cast<int*>(ipc_allocator.allocate(conn_bytes));
    int* conn_b = static_cast<int*>(ipc_allocator.allocate(conn_bytes));
    int* conn_c = static_cast<int*>(ipc_allocator.allocate(conn_bytes));
    int* conn_d = static_cast<int*>(ipc_allocator.allocate(conn_bytes));

    auto scope_comm = umpire::get_communicator_for_allocator(ipc_allocator, app_comm);
    int scope_rank;
    MPI_Comm_rank(scope_comm, &scope_rank);

    // Only the leader of each socket reads the file and fills the GPU arrays.
    if (scope_rank == 0) {
      double* h_coord = static_cast<double*>(host_allocator.allocate(3 * coord_bytes));
      int* h_conn = static_cast<int*>(host_allocator.allocate(4 * conn_bytes));

      double* h_x = h_coord;
      double* h_y = h_coord + num_nodes;
      double* h_z = h_coord + 2 * num_nodes;
      int* h_a = h_conn;
      int* h_b = h_conn + num_tets;
      int* h_c = h_conn + 2 * num_tets;
      int* h_d = h_conn + 3 * num_tets;

      std::ifstream in(filename);
      long unused_nodes, unused_tets;
      in >> unused_nodes >> unused_tets;

      for (std::size_t i = 0; i < num_nodes; i++) {
        in >> h_x[i] >> h_y[i] >> h_z[i];
      }
      for (std::size_t i = 0; i < num_tets; i++) {
        in >> h_a[i] >> h_b[i] >> h_c[i] >> h_d[i];
      }

      rm.copy(x, h_x, coord_bytes);
      rm.copy(y, h_y, coord_bytes);
      rm.copy(z, h_z, coord_bytes);
      rm.copy(conn_a, h_a, conn_bytes);
      rm.copy(conn_b, h_b, conn_bytes);
      rm.copy(conn_c, h_c, conn_bytes);
      rm.copy(conn_d, h_d, conn_bytes);

      host_allocator.deallocate(h_coord);
      host_allocator.deallocate(h_conn);
    }

    // Make sure the leader's copies are complete before anyone touches the
    // shared GPU memory.
    MPI_Barrier(scope_comm);

#if defined(UMPIRE_ENABLE_CUDA)
    cudaDeviceSynchronize();
#elif defined(UMPIRE_ENABLE_HIP)
    hipDeviceSynchronize();
#endif

    // Every rank now sees the mesh in GPU memory. Verify by copying the last
    // node and last tetrahedron back to the host.
    double* h_node = static_cast<double*>(host_allocator.allocate(3 * sizeof(double)));
    int* h_tet = static_cast<int*>(host_allocator.allocate(4 * sizeof(int)));

    rm.copy(&h_node[0], x + num_nodes - 1, sizeof(double));
    rm.copy(&h_node[1], y + num_nodes - 1, sizeof(double));
    rm.copy(&h_node[2], z + num_nodes - 1, sizeof(double));
    rm.copy(&h_tet[0], conn_a + num_tets - 1, sizeof(int));
    rm.copy(&h_tet[1], conn_b + num_tets - 1, sizeof(int));
    rm.copy(&h_tet[2], conn_c + num_tets - 1, sizeof(int));
    rm.copy(&h_tet[3], conn_d + num_tets - 1, sizeof(int));

    std::cout << "Rank " << app_rank << " (socket rank " << scope_rank << "): last node = (" << h_node[0] << ", "
              << h_node[1] << ", " << h_node[2] << "), last tet = [" << h_tet[0] << ", " << h_tet[1] << ", "
              << h_tet[2] << ", " << h_tet[3] << "]" << std::endl;

    host_allocator.deallocate(h_node);
    host_allocator.deallocate(h_tet);

    // Deallocation is also collective within each socket scope.
    ipc_allocator.deallocate(conn_d);
    ipc_allocator.deallocate(conn_c);
    ipc_allocator.deallocate(conn_b);
    ipc_allocator.deallocate(conn_a);
    ipc_allocator.deallocate(z);
    ipc_allocator.deallocate(y);
    ipc_allocator.deallocate(x);
  }

  MPI_Comm_free(&app_comm);
  MPI_Finalize();

  return 0;
}
