//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/util/numa.hpp"

#include <numa.h>
#include <numaif.h>
#include <unistd.h>

#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"

namespace umpire {

long get_page_size()
{
  static const long s_cpu_page_size = sysconf(_SC_PAGESIZE);
  return s_cpu_page_size;
}

namespace numa {

int preferred_node()
{
  if (numa_available() < 0)
    UMPIRE_ERROR(runtime_error, "libnuma is unusable.");
  return numa_preferred();
}

void move_to_node(void* ptr, std::size_t bytes, int node)
{
  if (numa_available() < 0)
    UMPIRE_ERROR(runtime_error, "libnuma is unusable.");

  struct bitmask* mask = numa_bitmask_alloc(numa_max_node() + 1);
  numa_bitmask_clearall(mask);
  numa_bitmask_setbit(mask, node);

  if (mbind(ptr, bytes, MPOL_BIND, mask->maskp, mask->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("numa::move_to_node error: mbind( ptr = {}, bytes = {}, node = {} ) failed: {}", ptr,
                             bytes, node, strerror(errno)));
  }

  numa_bitmask_free(mask);
}

int get_location(void* ptr)
{
  int numa_node = -1;
  if (get_mempolicy(&numa_node, NULL, 0, ptr, MPOL_F_NODE | MPOL_F_ADDR) != 0) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("numa::get_location error: get_mempolicy( ptr = {} ) failed: {}", ptr, strerror(errno)));
  }
  return numa_node;
}

int node_of_cpu(int cpu)
{
  if (numa_available() < 0)
    UMPIRE_ERROR(runtime_error, "libnuma is unusable.");

  const int numa_node = numa_node_of_cpu(cpu);
  if (numa_node < 0) {
    UMPIRE_ERROR(runtime_error, fmt::format("numa::node_of_cpu error for cpu {}", cpu));
  }

  return numa_node;
}

std::vector<int> get_host_nodes()
{
  if (numa_available() < 0)
    UMPIRE_ERROR(runtime_error, "libnuma is unusable.");

  std::vector<int> host_nodes;
  struct bitmask* cpus = numa_allocate_cpumask();

  const int size = numa_all_nodes_ptr->size;
  for (int i = 0; i < size; i++) {
    if (numa_bitmask_isbitset(numa_all_nodes_ptr, i)) {
      // Check if this has CPUs
      if (numa_node_to_cpus(i, cpus) != 0) {
        UMPIRE_ERROR(runtime_error, "Error getting CPU list for NUMA node.");
      }

      const std::size_t ncpus = numa_bitmask_weight(cpus);
      if (ncpus > 0) {
        // This is a host node
        host_nodes.push_back(i);
      }
    }
  }

  numa_free_cpumask(cpus);
  return host_nodes;
}

std::vector<int> get_device_nodes()
{
  if (numa_available() < 0)
    UMPIRE_ERROR(runtime_error, "libnuma is unusable.");

  std::vector<int> device_nodes;
  struct bitmask* cpus = numa_allocate_cpumask();

  const int size = numa_all_nodes_ptr->size;
  for (int i = 0; i < size; i++) {
    if (numa_bitmask_isbitset(numa_all_nodes_ptr, i)) {
      // Check if this has CPUs
      if (numa_node_to_cpus(i, cpus) != 0) {
        UMPIRE_ERROR(runtime_error, "Error getting CPU list for NUMA node.");
      }

      const std::size_t ncpus = numa_bitmask_weight(cpus);
      if (ncpus == 0) {
        // This is a device node
        device_nodes.push_back(i);
      }
    }
  }

  numa_free_cpumask(cpus);
  return device_nodes;
}

std::vector<int> get_allocatable_nodes()
{
  if (numa_available() < 0)
    UMPIRE_ERROR(runtime_error, "libnuma is unusable.");

  struct bitmask* mask = numa_get_mems_allowed();
  std::vector<int> nodes;
  const int size = mask->size;
  for (int i = 0; i < size; i++) {
    if (numa_bitmask_isbitset(mask, i))
      nodes.push_back(i);
  }

  numa_free_nodemask(mask);
  return nodes;
}

} // end namespace numa
} // end namespace umpire
