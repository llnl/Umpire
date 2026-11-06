//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "umpire/strategy/DeviceIpcAllocator.hpp"

#include <sstream>

#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/resource/HostSharedMemoryResource.hpp"
#include "umpire/strategy/NamedAllocationStrategy.hpp"
#include "umpire/util/MPI.hpp"
#include "umpire/util/MemoryResourceTraits.hpp"
#include "umpire/util/error.hpp"

#if defined(UMPIRE_ENABLE_CUDA)
#define gpuIpcGetMemHandle cudaIpcGetMemHandle
#define gpuIpcCloseMemHandle cudaIpcCloseMemHandle
#define gpuIpcOpenMemHandle cudaIpcOpenMemHandle
#define gpuGetDevice cudaGetDevice
#define gpuSetDevice cudaSetDevice
#define gpuError cudaError_t
#define gpuGetDeviceProperties cudaGetDeviceProperties
#define gpuGetErrorString cudaGetErrorString
#define gpuSuccess cudaSuccess
#define gpuDeviceProp cudaDeviceProp
#define gpuIpcMemLazyEnablePeerAccess cudaIpcMemLazyEnablePeerAccess
#elif defined(UMPIRE_ENABLE_HIP)
#define gpuIpcGetMemHandle hipIpcGetMemHandle
#define gpuIpcCloseMemHandle hipIpcCloseMemHandle
#define gpuIpcOpenMemHandle hipIpcOpenMemHandle
#define gpuGetDevice hipGetDevice
#define gpuSetDevice hipSetDevice
#define gpuError hipError_t
#define gpuGetDeviceProperties hipGetDeviceProperties
#define gpuGetErrorString hipGetErrorString
#define gpuSuccess hipSuccess
#define gpuDeviceProp hipDeviceProp_t
#define gpuIpcMemLazyEnablePeerAccess hipIpcMemLazyEnablePeerAccess
#endif

namespace umpire {
namespace strategy {

DeviceIpcAllocator::DeviceIpcAllocator(const std::string& name, int id) noexcept
    : DeviceIpcAllocator(name, id, umpire::ResourceManager::getInstance().getAllocator("DEVICE"),
                         MemoryResourceTraits::shared_scope::socket)
{
}

DeviceIpcAllocator::DeviceIpcAllocator(const std::string& name, int id, Allocator device_allocator,
                                       MemoryResourceTraits::shared_scope scope,
                                       std::size_t shared_memory_size) noexcept
    : AllocationStrategy(name, id, device_allocator.getAllocationStrategy(), "DeviceIpcAllocator"),
      m_device_allocator(device_allocator.getAllocationStrategy()),
      m_scope_rank(0),
      m_is_scope_leader(false),
      m_scope_color(MPI_COMM_TYPE_SHARED),
      m_scope(scope)
{
  setup_shared_scope(scope);

  auto& rm = umpire::ResourceManager::getInstance();
  auto traits = umpire::get_default_resource_traits("SHARED");
  traits.size = shared_memory_size;
  auto shared_alloc = rm.makeResource("SHARED::" + name + std::to_string(m_scope_color), traits);
  m_shared_allocator = shared_alloc.getAllocationStrategy();
  m_shared_memory_resource = dynamic_cast<umpire::resource::HostSharedMemoryResource*>(m_shared_allocator);
}

DeviceIpcAllocator::~DeviceIpcAllocator()
{
  MPI_Barrier(m_scope_comm);

  if (m_is_scope_leader) {
    for (auto& item : m_allocation_names) {
      try {
        m_device_allocator->deallocate_internal(item.first);
      } catch (const std::exception& e) {
        UMPIRE_LOG(Warning, fmt::format("Exception during cleanup: {}", e.what()));
      }
    }
    m_allocation_names.clear();
  }

  MPI_Comm_free(&m_scope_comm);
}

void DeviceIpcAllocator::setup_shared_scope(MemoryResourceTraits::shared_scope scope)
{
  if (scope == MemoryResourceTraits::shared_scope::node) {
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &m_scope_comm);
    MPI_Comm_rank(m_scope_comm, &m_scope_rank);
    UMPIRE_LOG(Debug, fmt::format("Node scope: rank {} in node communicator", m_scope_rank));
  } else if (scope == MemoryResourceTraits::shared_scope::socket) {
    // Get current device
    int device_id;
    gpuError err = gpuGetDevice(&device_id);
    if (err != gpuSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("gpuGetDevice failed with error: {}", gpuGetErrorString(err)));
    }

    // Get device properties
    gpuDeviceProp props;
    err = gpuGetDeviceProperties(&props, device_id);
    if (err != gpuSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("gpuGetDeviceProperties failed with error: {}", gpuGetErrorString(err)));
    }

    // Use PCI domain, bus, and device as color for MPI communicator split
    m_scope_color = props.pciDomainID * 1000000 + props.pciBusID * 1000 + props.pciDeviceID;

    UMPIRE_LOG(Debug, fmt::format("Socket scope: using color {} for PCI split", m_scope_color));

    //MPI_Comm_split(MPI_COMM_WORLD, m_scope_color, 0, &m_scope_comm);
    MPI_Comm node_comm;
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &node_comm);
    MPI_Comm_split(node_comm, m_scope_color, 0, &m_scope_comm);
    MPI_Comm_free(&node_comm);

    MPI_Comm_rank(m_scope_comm, &m_scope_rank);
    UMPIRE_LOG(Debug, fmt::format("Socket scope: rank {} in socket communicator", m_scope_rank));
  } else {
    UMPIRE_ERROR(runtime_error, "Unsupported scope for DeviceIpcAllocator");
  }

  m_is_scope_leader = (m_scope_rank == 0);
  UMPIRE_LOG(Debug, fmt::format("Rank {} is {} leader", m_scope_rank, m_is_scope_leader ? "scope" : "not scope"));
}

std::string DeviceIpcAllocator::generate_allocation_name(std::size_t size_in_bytes)
{
  static unsigned long counter = 0;
  std::stringstream ss;
  ss << m_name << "_" << m_scope_color << "_" << size_in_bytes << "_" << counter++;
  return ss.str();
}

void* DeviceIpcAllocator::allocate(std::size_t bytes)
{
  UMPIRE_LOG(Debug, fmt::format("(size_in_bytes={})", bytes));

  std::string allocation_name = generate_allocation_name(bytes);

  void* ptr = nullptr;

  if (m_is_scope_leader) {
    ptr = create(allocation_name, bytes);
  } else {
    ptr = import(allocation_name);
  }

  if (!ptr) {
    UMPIRE_ERROR(runtime_error, "Failed to allocate/import device memory");
  }

  // Track allocation name (only in leader process)
  if (m_is_scope_leader) {
    m_allocation_names[ptr] = allocation_name;
  }

  return ptr;
}

void* DeviceIpcAllocator::create(const std::string& name, std::size_t size_in_bytes)
{
  void* ptr = m_device_allocator->allocate_internal(size_in_bytes);
  UMPIRE_LOG(Debug, fmt::format("Leader allocated device memory at {} (size: {})", ptr, size_in_bytes));

  IpcHandleInfo* handle_info = create_handle_info(name, size_in_bytes);
  if (!handle_info) {
    m_device_allocator->deallocate_internal(ptr);
    UMPIRE_ERROR(runtime_error, "Failed to create handle info in shared memory");
  }

  auto err = gpuIpcGetMemHandle(&handle_info->handle, ptr);
  if (err != gpuSuccess) {
    m_device_allocator->deallocate_internal(ptr);
    m_shared_allocator->deallocate_internal(handle_info);
    UMPIRE_ERROR(runtime_error, fmt::format("gpuIpcGetMemHandle failed with error: {}", gpuGetErrorString(err)));
  }

  // Setup handle info fields
  handle_info->size = size_in_bytes;
  err = gpuGetDevice(&handle_info->device_id);
  if (err != gpuSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("gpuGetDevice failed with error: {}", gpuGetErrorString(err)));
  }
  handle_info->is_initialized.store(true, std::memory_order_release);

  // Signal followers that the handle is ready
  MPI_Barrier(m_scope_comm);
  UMPIRE_LOG(Debug, fmt::format("Leader completed IPC setup for device memory at {}", ptr));

  return ptr;
}

void* DeviceIpcAllocator::import(const std::string& name)
{
  // Wait for leader to initialize the handle
  MPI_Barrier(m_scope_comm);

  IpcHandleInfo* handle_info = get_handle_info(name);
  if (!handle_info || !handle_info->is_initialized.load(std::memory_order_acquire)) {
    UMPIRE_ERROR(runtime_error, "Failed to get initialized IPC handle");
    return nullptr;
  }

  int current_device, target_device = handle_info->device_id;
  gpuError err = gpuGetDevice(&current_device);
  if (err != gpuSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("gpuGetDevice failed with error: {}", gpuGetErrorString(err)));
  }
  bool device_switched = false;
  if (current_device != target_device) {
    err = gpuSetDevice(target_device);
    if (err != gpuSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("gpuSetDevice failed with error: {}", gpuGetErrorString(err)));
    }
    device_switched = true;
  }

  void* ptr = nullptr;
  err = gpuIpcOpenMemHandle(&ptr, handle_info->handle, gpuIpcMemLazyEnablePeerAccess);
  UMPIRE_LOG(Debug, fmt::format("Follower opened IPC handle to device memory at {}", ptr));
  if (err != gpuSuccess) {
    auto store_error_temp = gpuGetErrorString(err);
    if (device_switched) {
      err = gpuSetDevice(current_device);
      if (err != gpuSuccess) {
        UMPIRE_ERROR(runtime_error, fmt::format("gpuSetDevice failed with error: {}", gpuGetErrorString(err)));
      }
    }
    UMPIRE_ERROR(runtime_error, fmt::format("gpuIpcOpenMemHandle failed with error: {}", store_error_temp));
    return nullptr;
  }

  if (device_switched) {
    err = gpuSetDevice(current_device);
    if (err != gpuSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("gpuSetDevice failed with error: {}", gpuGetErrorString(err)));
    }
  }

  UMPIRE_LOG(Debug, fmt::format("Follower successfully imported device memory at {}", ptr));
  return ptr;
}

void DeviceIpcAllocator::deallocate(void* ptr, std::size_t)
{
  UMPIRE_LOG(Debug, fmt::format("(ptr={})", ptr));

  MPI_Barrier(m_scope_comm);

  if (m_is_scope_leader) {
    auto it = m_allocation_names.find(ptr);
    if (it == m_allocation_names.end()) {
      UMPIRE_ERROR(runtime_error, "Cannot deallocate unknown pointer");
    }

    const std::string& allocation_name = it->second;

    m_device_allocator->deallocate_internal(ptr);

    IpcHandleInfo* handle_info = get_handle_info(allocation_name);
    if (handle_info) {
      m_shared_allocator->deallocate_internal(handle_info);
    }

    m_allocation_names.erase(it);
  } else {
    gpuError err = gpuIpcCloseMemHandle(ptr);
    if (err != gpuSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("gpuIpcCloseMemHandle failed with error: {}", gpuGetErrorString(err)));
    }
  }
}

DeviceIpcAllocator::IpcHandleInfo* DeviceIpcAllocator::get_handle_info(const std::string& name)
{
  void* shared_ptr = m_shared_memory_resource->find_pointer_from_name(name);
  UMPIRE_LOG(Debug, fmt::format("Found shared memory at {} for {}", shared_ptr, name));
  return static_cast<IpcHandleInfo*>(shared_ptr);
}

DeviceIpcAllocator::IpcHandleInfo* DeviceIpcAllocator::create_handle_info(const std::string& name,
                                                                          std::size_t size_in_bytes)
{
  if (m_is_scope_leader) {
    void* shared_ptr = m_shared_memory_resource->allocate_named_internal(name, sizeof(IpcHandleInfo));
    UMPIRE_LOG(Debug, fmt::format("Leader created shared memory at {} for {}", shared_ptr, name));

    if (shared_ptr) {
      IpcHandleInfo* info = static_cast<IpcHandleInfo*>(shared_ptr);
      info->size = size_in_bytes;
      info->device_id = 0;
      info->is_initialized.store(false, std::memory_order_relaxed);
      return info;
    }
  }

  return nullptr;
}

MPI_Comm DeviceIpcAllocator::get_scope_communicator()
{
  return m_scope_comm;
}

Platform DeviceIpcAllocator::getPlatform() noexcept
{
  return m_device_allocator->getPlatform();
}

MemoryResourceTraits DeviceIpcAllocator::getTraits() const noexcept
{
  auto traits = m_device_allocator->getTraits();
  traits.scope = m_scope;
  traits.ipc = true;
  return traits;
}

} // end of namespace strategy
} // end of namespace umpire
