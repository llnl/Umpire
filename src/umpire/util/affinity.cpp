//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "umpire/util/affinity.hpp"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>

#if defined(__linux__)
#include <sched.h>
#include <unistd.h>
#endif

#include "fmt/format.h"

#include "umpire/config.hpp"
#include "umpire/util/Macros.hpp"
#if defined(UMPIRE_ENABLE_NUMA)
#include "umpire/util/numa.hpp"
#endif

namespace umpire {
namespace util {

namespace {

#if defined(__linux__)
bool try_get_socket_id_for_cpu(int cpu, int& socket_id)
{
  std::ifstream package_id_file{"/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/physical_package_id"};
  if (package_id_file) {
    package_id_file >> socket_id;
    if (package_id_file) {
      UMPIRE_LOG(Debug, "Using Linux physical_package_id " << socket_id << " for cpu " << cpu);
      return true;
    }

    UMPIRE_LOG(Debug, "Read package ID for file but could not read physical_package_id for cpu " << cpu);
  } else {
    UMPIRE_LOG(Debug, "Could not open physical_package_id for cpu " << cpu);
  }

#if defined(UMPIRE_ENABLE_NUMA)
  try {
    socket_id = numa::node_of_cpu(cpu);
    UMPIRE_LOG(Debug, "Using NUMA node " << socket_id << " as socket id fallback for cpu " << cpu);
    return true;
  } catch (const std::exception& e) {
    UMPIRE_LOG(Debug, "NUMA fallback failed for cpu " << cpu << ": " << e.what());
  }
#endif

  UMPIRE_USE_VAR(cpu);
  UMPIRE_USE_VAR(socket_id);
  return false;
}

std::string format_socket_ids(const std::set<int>& socket_ids)
{
  std::ostringstream ss;

  bool first = true;
  for (int socket_id : socket_ids) {
    if (!first) {
      ss << ", ";
    }
    ss << socket_id;
    first = false;
  }

  return ss.str();
}
#endif

} // end anonymous namespace

bool get_socket_id_from_affinity(int& socket_id, std::string& reason)
{
#if defined(__linux__)
  const long cpu_count_long = sysconf(_SC_NPROCESSORS_CONF);
  if (cpu_count_long <= 0) {
    reason = "Could not determine configured CPU count (sysconf(_SC_NPROCESSORS_CONF) failed)";
    return false;
  }

  if (cpu_count_long > std::numeric_limits<int>::max()) {
    reason = "Configured CPU count exceeds supported range";
    return false;
  }

  const int cpu_count = static_cast<int>(cpu_count_long);

  const auto set_size = CPU_ALLOC_SIZE(cpu_count);
  cpu_set_t* cpu_set = CPU_ALLOC(cpu_count);
  if (!cpu_set) {
    reason = "Failed to allocate CPU affinity mask";
    return false;
  }

  CPU_ZERO_S(set_size, cpu_set);

  if (sched_getaffinity(0, set_size, cpu_set) != 0) {
    reason = fmt::format("sched_getaffinity failed: {}", std::strerror(errno));
    CPU_FREE(cpu_set);
    return false;
  }

  std::set<int> socket_ids;
  bool saw_cpu = false;

  for (int cpu = 0; cpu < cpu_count; ++cpu) {
    if (!CPU_ISSET_S(cpu, set_size, cpu_set)) {
      continue;
    }
    saw_cpu = true;

    int current_socket_id{-1};
    if (!try_get_socket_id_for_cpu(cpu, current_socket_id)) {
      reason = "Unable to determine socket identifier for a CPU in this rank's affinity mask";
      CPU_FREE(cpu_set);
      return false;
    }

    socket_ids.insert(current_socket_id);
    if (socket_ids.size() > 1) {
      reason =
          fmt::format("Socket-scoped MPI3 shared memory requires each rank pinned to exactly one socket; this rank spans "
                      "multiple sockets ({})",
                      format_socket_ids(socket_ids));
      CPU_FREE(cpu_set);
      return false;
    }
  }

  CPU_FREE(cpu_set);

  if (!saw_cpu) {
    reason = "Rank CPU affinity mask is empty";
    return false;
  }

  if (socket_ids.empty()) {
    reason = "No socket identifiers found in affinity mask";
    return false;
  }

  socket_id = *socket_ids.begin();
  return true;
#else
  UMPIRE_USE_VAR(socket_id);
  reason = "Socket-scoped MPI3 shared memory requires Linux CPU affinity information";
  return false;
#endif
}

bool affinity_maps_to_single_socket(std::string& reason)
{
  int socket_id{-1};
  return get_socket_id_from_affinity(socket_id, reason);
}

} // end namespace util
} // end namespace umpire
