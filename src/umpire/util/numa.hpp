//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_numa_HPP
#define UMPIRE_numa_HPP

#include <cstddef>
#include <vector>

namespace umpire {

// Return the page size
long get_page_size();

namespace numa {

// Return the preferred numa node
int preferred_node();

// Move page-aligned address of size bytes to node
void move_to_node(void* ptr, std::size_t bytes, int node);

// Return the numa node where address ptr resides
int get_location(void* ptr);

// Return the NUMA node associated with a CPU
int node_of_cpu(int cpu);

// List host NUMA nodes
std::vector<int> get_host_nodes();

// List device NUMA nodes
std::vector<int> get_device_nodes();

// List NUMA nodes that can be used from this process
std::vector<int> get_allocatable_nodes();

} // end namespace numa

} // end namespace umpire

#endif // UMPIRE_numa_HPP
