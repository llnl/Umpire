//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "umpire/Umpire.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <string>

#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"
#if defined(UMPIRE_ENABLE_MPI3_SHARED_MEMORY)
#include "umpire/resource/HostMpi3SharedMemoryResource.hpp"
#endif
#if defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY)
#include "umpire/resource/HostSharedMemoryResource.hpp"
#endif
#include "umpire/resource/MemoryResource.hpp"
#if defined(UMPIRE_ENABLE_MPI) && defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY)
#if defined(UMPIRE_ENABLE_DEVICE)
#include "umpire/strategy/DeviceIpcAllocator.hpp"
#endif
#endif
#include "umpire/strategy/DynamicPoolList.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/ResourceAwarePool.hpp"
#include "umpire/util/wrap_allocator.hpp"

#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include <fstream>
#include <sstream>

UMPIRE_EXPORT int UMPIRE_VERSION_SYM{0};

namespace umpire {

#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)

void print_allocator_records(Allocator allocator, std::ostream&)
{
  // Allocation headers cannot be enumerated
  UMPIRE_LOG(Info, "print_allocator_records for \""
                       << allocator.getName()
                       << "\" is not supported with UMPIRE_ENABLE_INTROSPECTION_HEADER");
}

std::vector<util::AllocationRecord> get_allocator_records(Allocator allocator)
{
  // Allocation headers cannot be enumerated
  UMPIRE_LOG(Info, "get_allocator_records for \""
                       << allocator.getName()
                       << "\" is not supported with UMPIRE_ENABLE_INTROSPECTION_HEADER");

  return {};
}

#else

void print_allocator_records(Allocator allocator, std::ostream& os)
{
  std::stringstream ss;
  auto& rm = umpire::ResourceManager::getInstance();

  auto strategy = allocator.getAllocationStrategy();

  rm.m_allocations.print([strategy](const util::AllocationRecord& rec) { return rec.strategy == strategy; }, ss);

  if (!ss.str().empty()) {
    os << "Allocations for " << allocator.getName() << " allocator:" << std::endl << ss.str() << std::endl;
  }
}

std::vector<util::AllocationRecord> get_allocator_records(Allocator allocator)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto strategy = allocator.getAllocationStrategy();

  std::vector<util::AllocationRecord> recs;
  std::copy_if(rm.m_allocations.begin(), rm.m_allocations.end(), std::back_inserter(recs),
               [strategy](const util::AllocationRecord& rec) { return rec.strategy == strategy; });

  return recs;
}

#endif // UMPIRE_ENABLE_INTROSPECTION_HEADER

bool pointer_overlaps(void* left_ptr, void* right_ptr)
{
  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto left_record = rm.findAllocationRecord(left_ptr);
    auto right_record = rm.findAllocationRecord(right_ptr);

    char* left{reinterpret_cast<char*>(left_record->ptr)};
    char* right{reinterpret_cast<char*>(right_record->ptr)};

    return ((right >= left) && ((left + left_record->size) > right) &&
            ((right + right_record->size) > (left + left_record->size)));
  } catch (umpire::runtime_error&) {
    UMPIRE_LOG(Error, "Unknown pointer in pointer_overlaps");
    throw;
  }
}

bool pointer_contains(void* left_ptr, void* right_ptr)
{
  auto& rm = umpire::ResourceManager::getInstance();

  try {
    auto left_record = rm.findAllocationRecord(left_ptr);
    auto right_record = rm.findAllocationRecord(right_ptr);

    char* left{reinterpret_cast<char*>(left_record->ptr)};
    char* right{reinterpret_cast<char*>(right_record->ptr)};

    return ((right >= left) && (left + left_record->size > right) &&
            (right + right_record->size <= left + left_record->size));
  } catch (umpire::runtime_error&) {
    UMPIRE_LOG(Error, "Unknown pointer in pointer_contains");
    throw;
  }
}

bool is_accessible(Platform p, Allocator a)
{
  // get base (parent) resource
  umpire::strategy::AllocationStrategy* root = a.getAllocationStrategy();
  while ((root->getParent() != nullptr)) {
    root = root->getParent();
  }

  // unwrap the base MemoryResource and return whether or not it's accessible
  umpire::resource::MemoryResource* resource = util::unwrap_allocation_strategy<umpire::resource::MemoryResource>(root);
  return resource->isAccessibleFrom(p);
}

std::string get_backtrace(void* ptr)
{
#if defined(UMPIRE_ENABLE_BACKTRACE)
  auto& rm = umpire::ResourceManager::getInstance();
  auto record = rm.findAllocationRecord(ptr);
  return umpire::util::backtracer<>::print(record->allocation_backtrace);
#else
  UMPIRE_USE_VAR(ptr);
  return "[Umpire: UMPIRE_BACKTRACE=Off]";
#endif
}

std::size_t get_process_memory_usage_hwm()
{
#if defined(_MSC_VER) || defined(__APPLE__)
  return 0;
#else
  std::ifstream status{"/proc/self/status"};

  std::size_t rval{0};
  std::string line;
  while (std::getline(status, line)) {
    std::stringstream ss{line};
    std::string key;
    ss >> key;

    if (key == "VmHWM:") {
      std::size_t resident_hwm;
      ss >> resident_hwm;

      //
      // "VmHWM" returns the number of kB in use. Convert this to number of bytes
      //
      rval = std::size_t{resident_hwm * 1024};
      break;
    }
  }
  return rval;
#endif
}

std::size_t get_process_memory_usage()
{
#if defined(_MSC_VER) || defined(__APPLE__)
  return 0;
#else
  std::size_t ignore;
  std::size_t resident;
  std::ifstream statm("/proc/self/statm");
  statm >> ignore >> resident >> ignore;
  statm.close();
  long page_size{::sysconf(_SC_PAGE_SIZE)};
  return std::size_t{resident * page_size};
#endif
}

std::size_t get_mapping_memory_usage(const std::string& mapping_name)
{
#if defined(__linux__)
  std::ifstream smaps{"/proc/self/smaps"};
  if (!smaps) {
    return 0;
  }

  std::size_t rss_kb{0};
  bool in_target_mapping{false};
  std::string line;

  while (std::getline(smaps, line)) {
    // Memory mapping header lines follow the format: "address-address perms ..."
    // Check if line starts with hex digits and contains a hyphen in the first field
    const bool is_header = (!line.empty() && std::isxdigit(static_cast<unsigned char>(line[0])) &&
                            line.find('-') != std::string::npos && line.find('-') < 20);

    if (is_header) {
      in_target_mapping = (line.find(mapping_name) != std::string::npos);
      continue;
    }

    if (!in_target_mapping || line.rfind("Rss:", 0) != 0) {
      continue;
    }

    std::size_t value_kb{0};
    std::sscanf(line.c_str(), "Rss: %zu kB", &value_kb);
    rss_kb += value_kb;
  }

  return rss_kb * 1024;
#else
  UMPIRE_USE_VAR(mapping_name);
  return 0;
#endif
}

std::size_t get_internal_memory_usage()
{
  return umpire::ResourceManager::getInstance().getInternalMemoryUsage();
}

void mark_event(const std::string& event)
{
  umpire::event::record(
      [&](auto& e) { e.name("event").category(event::category::metadata).arg("name", event).tag("replay", "true"); });
}

std::size_t get_total_bytes_allocated()
{
  auto& rm = umpire::ResourceManager::getInstance();
  std::size_t total_memory{0};

  for (auto s : rm.getResourceNames()) {
    umpire::Allocator alloc = rm.getAllocator(s);
    total_memory += alloc.getActualSize();
  }

  for (auto s : rm.getSharedAllocatorNames()) {
    umpire::Allocator alloc = rm.getAllocator(s);
    total_memory += alloc.getActualSize();
  }

  return total_memory;
}

std::size_t get_device_memory_usage(int device_id)
{
#if defined(UMPIRE_ENABLE_CUDA)
  std::size_t mem_free{0};
  std::size_t mem_tot{0};

  int current_device;
  cudaError_t err = cudaGetDevice(&current_device);
  if (err != cudaSuccess) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("cudaGetDevice failed with error: {}", cudaGetErrorString(err)));
  }

  err = cudaSetDevice(device_id);
  if (err != cudaSuccess) {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Error when trying to set CUDA Device: {}", cudaGetErrorString(err)));
  }

  err = cudaMemGetInfo(&mem_free, &mem_tot);
  if (err != cudaSuccess) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("cudaMemGetInfo failed with error: {}", cudaGetErrorString(err)));
  }

  err = cudaSetDevice(current_device);
  if (err != cudaSuccess) {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Error when trying to set CUDA Device: {}", cudaGetErrorString(err)));
  }

  return std::size_t{mem_tot - mem_free};
#elif defined(UMPIRE_ENABLE_HIP)
  std::size_t mem_free{0};
  std::size_t mem_tot{0};

  int current_device;
  hipError_t err = hipGetDevice(&current_device);
  if (err != hipSuccess) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("hipGetDevice failed with error: {}", hipGetErrorString(err)));
  }

  err = hipSetDevice(device_id);
  if (err != hipSuccess) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("Error when trying to set HIP Device: {}", hipGetErrorString(err)));
  }

  err = hipMemGetInfo(&mem_free, &mem_tot);
  if (err != hipSuccess) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("hipMemGetInfo failed with error: {}", hipGetErrorString(err)));
  }

  err = hipSetDevice(current_device);
  if (err != hipSuccess) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("Error when trying to set HIP Device: {}", hipGetErrorString(err)));
  }

  return std::size_t{mem_tot - mem_free};
#endif
  UMPIRE_USE_VAR(device_id);
  return 0;
}

std::vector<util::AllocationRecord> get_leaked_allocations(Allocator allocator)
{
  return get_allocator_records(allocator);
}

umpire::MemoryResourceTraits get_default_resource_traits(const std::string& name)
{
  umpire::resource::MemoryResourceRegistry& registry{umpire::resource::MemoryResourceRegistry::getInstance()};
  umpire::MemoryResourceTraits traits(registry.getDefaultTraitsForResource(name));
  return traits;
}

void* find_pointer_from_name(Allocator allocator, const std::string& name)
{
  void* ptr{nullptr};

#if defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY)
  auto base_strategy = util::unwrap_allocator<strategy::AllocationStrategy>(allocator);

  umpire::resource::HostSharedMemoryResource* shared_resource =
      reinterpret_cast<umpire::resource::HostSharedMemoryResource*>(base_strategy);

  if (shared_resource != nullptr) {
    ptr = shared_resource->find_pointer_from_name(name);
  } else
#else
  UMPIRE_USE_VAR(name);

  // Using unused attribute to silence warning from rocm compiler
  UMPIRE_USE_VAR(allocator);
#endif // defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY)

  {
    if (ptr == nullptr) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Allocator \"{}\" is not a Shared Memory Allocator", allocator.getName()));
    }
  }
  return ptr;
}

#if defined(UMPIRE_ENABLE_MPI)
namespace {
std::map<int, MPI_Comm>& get_cached_communicators()
{
  static std::map<int, MPI_Comm> cached_communicators{};
  return cached_communicators;
}
} // namespace

MPI_Comm get_communicator_for_allocator(Allocator a, MPI_Comm comm)
{
#if defined(UMPIRE_ENABLE_IPC_SHARED_MEMORY) && defined(UMPIRE_ENABLE_DEVICE)
  if (auto alloc = dynamic_cast<strategy::DeviceIpcAllocator*>(a.getAllocationStrategy()))
    return alloc->get_scope_communicator();
#endif
#if defined(UMPIRE_ENABLE_MPI3_SHARED_MEMORY)
  if (auto resource = dynamic_cast<resource::HostMpi3SharedMemoryResource*>(a.getAllocationStrategy()))
    return resource->getSharedCommunicator();
#endif

  std::map<int, MPI_Comm>& cached_communicators = get_cached_communicators();

  MPI_Comm c;
  auto scope = a.getAllocationStrategy()->getTraits().scope;
  int id = a.getId();

  auto cached_comm = cached_communicators.find(id);
  if (cached_comm != cached_communicators.end()) {
    c = cached_comm->second;
  } else {
    if (scope == MemoryResourceTraits::shared_scope::node) {
      MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &c);
    } else {
      c = MPI_COMM_NULL;
    }
    cached_communicators[id] = c;
  }

  return c;
}

void cleanup_cached_communicators()
{
  std::map<int, MPI_Comm>& comm = get_cached_communicators();

  for (auto c : comm) {
    if (c.second != MPI_COMM_NULL) {
      MPI_Comm_free(&c.second);
    }
  }

  comm.clear();
}
#endif

#if defined(UMPIRE_ENABLE_MPI)
bool can_use_socket_scoped_mpi3_shared_memory(MPI_Comm comm, std::string& reason)
{
#if defined(UMPIRE_ENABLE_MPI3_SHARED_MEMORY)
  const int local_affinity_valid = resource::affinity_maps_to_single_socket(reason) ? 1 : 0;
  int all_affinity_valid{0};
  const int status = MPI_Allreduce(&local_affinity_valid, &all_affinity_valid, 1, MPI_INT, MPI_MIN, comm);

  if (status != MPI_SUCCESS) {
    reason = fmt::format("MPI_Allreduce failed while checking socket affinity: {}",
                         util::get_mpi_error_message(status));
    return false;
  }

  if (!all_affinity_valid && local_affinity_valid) {
    reason = "Another rank in the communicator does not map to a single socket";
  }

  return all_affinity_valid != 0;
#else
  UMPIRE_USE_VAR(comm);
  reason = "MPI3 shared memory support is disabled";
  return false;
#endif
}
#endif

void register_external_allocation(void* ptr, util::AllocationRecord record)
{
  umpire::event::record([&](auto& event) {
    event.name("register_external_allocation")
        .category(event::category::operation)
        .arg("allocator_ref", (void*)record.strategy)
        .arg("size", record.size)
        .arg("pointer", record.ptr)
        .tag("allocator_name", record.strategy->getName())
        .tag("replay", "true");
  });

  auto& rm = umpire::ResourceManager::getInstance();
  rm.registerAllocation(ptr, record);
}

util::AllocationRecord deregister_external_allocation(void* ptr)
{
  umpire::event::record([&](auto& event) {
    event.name("deregister_external_allocation").category(event::category::operation).tag("replay", "true");
  });

  auto& rm = umpire::ResourceManager::getInstance();
  return rm.deregisterAllocation(ptr);
}

camp::resources::Resource get_resource(Allocator a, void* ptr)
{
  UMPIRE_LOG(Warning,
             "This function will return a generic Camp resource which is not comparable to a specific Camp resource!");

  auto s = a.getAllocationStrategy();
  strategy::ResourceAwarePool* rap{dynamic_cast<strategy::ResourceAwarePool*>(s)};

  if (!rap) {
    UMPIRE_ERROR(runtime_error, fmt::format("Allocator \"{}\" is not a ResourceAwarePool!", a.getName()));
  }

  return rap->getResource(ptr);
}

std::size_t get_num_pending(Allocator a)
{
  auto s = a.getAllocationStrategy();
  strategy::ResourceAwarePool* rap{dynamic_cast<strategy::ResourceAwarePool*>(s)};

  if (!rap) {
    UMPIRE_ERROR(runtime_error, fmt::format("Allocator \"{}\" is not a ResourceAwarePool!", a.getName()));
  }

  return rap->getNumPending();
}

bool try_coalesce(Allocator a)
{
  auto s = a.getAllocationStrategy();
  bool coalesced{false};

  strategy::QuickPool* qp{dynamic_cast<strategy::QuickPool*>(s)};
  if (qp) {
    qp->coalesce();
    coalesced = true;
  }

  strategy::DynamicPoolList* dpl{dynamic_cast<strategy::DynamicPoolList*>(s)};
  if (dpl) {
    dpl->coalesce();
    coalesced = true;
  }

  return coalesced;
}

void coalesce(Allocator a)
{
  bool coalesced{try_coalesce(a)};

  if (!coalesced)
    UMPIRE_ERROR(runtime_error, fmt::format("Allocator \"{}\" could not be coalesced", a.getName()));
}

} // end namespace umpire
