//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/ResourceManager.hpp"

#include <iterator>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <algorithm>

#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/op/MemoryOperation.hpp"
#include "umpire/op/MemoryOperationRegistry.hpp"
#include "umpire/resource/MemoryResourceRegistry.hpp"
#include "umpire/strategy/FixedPool.hpp"
#if defined(UMPIRE_ENABLE_NUMA)
#include "umpire/strategy/NumaPolicy.hpp"
#endif
#include "umpire/util/MPI.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/io.hpp"
#include "umpire/util/make_unique.hpp"
#include "umpire/util/wrap_allocator.hpp"

#if defined(UMPIRE_ENABLE_CUDA)
#include <cuda_runtime_api.h>
#if defined(UMPIRE_ENABLE_DEVICE_ALLOCATOR)
#include "umpire/device_allocator_helper.hpp"
#endif
#endif

#if defined(UMPIRE_ENABLE_HIP)
#include <hip/hip_runtime.h>
#endif

#if defined(UMPIRE_ENABLE_SYCL)
#include "umpire/util/sycl_compat.hpp"
#endif

#include "umpire/op.hpp"

static const char* s_null_resource_name{"__umpire_internal_null"};
static const char* s_zero_byte_pool_name{"__umpire_internal_0_byte_pool"};

namespace umpire {

ResourceManager& ResourceManager::getInstance()
{
  static ResourceManager resource_manager;

  UMPIRE_LOG(Debug, "() returning " << &resource_manager);
  return resource_manager;
}

ResourceManager::ResourceManager()
    : m_allocations(),
      m_allocators(),
      m_allocators_by_id(),
      m_allocators_by_name(),
      m_memory_resources(),
      m_id(0),
      m_mutex()
{
  UMPIRE_LOG(Debug, "() entering");

  const char* env_enable_log{std::getenv("UMPIRE_LOG_LEVEL")};
  const bool enable_log{env_enable_log != nullptr};

  util::initialize_io(enable_log);

  initialize();

  UMPIRE_LOG(Debug, "() leaving");
}

ResourceManager::~ResourceManager()
{
  for (auto&& allocator : m_allocators) {
    if (allocator->getCurrentSize() != 0) {
      std::stringstream ss;

      umpire::print_allocator_records(Allocator{allocator.get()}, ss);

      UMPIRE_LOG(Error, allocator->getName()
                            << " Allocator still has " << allocator->getCurrentSize() << " bytes allocated" << std::endl
                            << ss.str() << std::endl);
    }

    allocator.reset();
  }
}

void ResourceManager::initialize()
{
  UMPIRE_LOG(Debug, "() entering");

  UMPIRE_LOG(Debug, "Umpire v" << UMPIRE_VERSION_MAJOR << "." << UMPIRE_VERSION_MINOR << "." << UMPIRE_VERSION_PATCH
                               << "." << UMPIRE_VERSION_RC);

  umpire::event::record([&](auto& event) {
    event.name("version")
        .category(event::category::metadata)
        .arg("major", UMPIRE_VERSION_MAJOR)
        .arg("minor", UMPIRE_VERSION_MINOR)
        .arg("patch", UMPIRE_VERSION_PATCH)
        .arg("rc", UMPIRE_VERSION_RC);
  });

  resource::MemoryResourceRegistry& registry{resource::MemoryResourceRegistry::getInstance()};

  {
    std::unique_ptr<strategy::AllocationStrategy> allocator{
        // util::wrap_allocator<strategy::AllocationTracker>(
        registry.makeMemoryResource(s_null_resource_name, getNextId())};

    m_null_allocator = allocator.get();
    m_allocators.emplace_front(std::move(allocator));
  }

  {
    std::unique_ptr<strategy::AllocationStrategy> allocator{
        new strategy::FixedPool{s_zero_byte_pool_name, getNextId(), Allocator{m_null_allocator}, 1}};

    m_zero_byte_pool = allocator.get();
    m_allocators.emplace_front(std::move(allocator));
  }

  UMPIRE_LOG(Debug, "() leaving");
}

Allocator ResourceManager::makeResource(const std::string& name)
{
  resource::MemoryResourceRegistry& registry{resource::MemoryResourceRegistry::getInstance()};
  return makeResource(name, registry.getDefaultTraitsForResource(name));
}

Allocator ResourceManager::makeResource(const std::string& name, MemoryResourceTraits traits)
{
  if (m_allocators_by_name.find(name) != m_allocators_by_name.end()) {
    UMPIRE_ERROR(runtime_error, fmt::format("Allocator \"{}\" already exists, and cannot be re-created.", name));
  }

  resource::MemoryResourceRegistry& registry{resource::MemoryResourceRegistry::getInstance()};

  if (name.find("DEVICE") != std::string::npos) {
    traits.id = resource::resource_to_device_id(name);
  }

  if (name.find("::COARSE") != std::string::npos) {
    traits.granularity = MemoryResourceTraits::granularity_type::coarse_grained;
  }

  if (name.find("::FINE") != std::string::npos) {
    traits.granularity = MemoryResourceTraits::granularity_type::fine_grained;
  }

  if (name.find("SHARED") != std::string::npos) {
    m_shared_allocator_names.push_back(name);
  }

  std::unique_ptr<strategy::AllocationStrategy> allocator{registry.makeMemoryResource(name, getNextId(), traits)};
  allocator->setTracking(traits.tracking);

  umpire::event::record([&](auto& event) {
    event.name("make_memory_resource")
        .category(event::category::operation)
        .arg("allocator_ref", (void*)allocator.get())
        .arg("introspection", traits.tracking)
        .tag("allocator_name", name)
        .tag("replay", "true");
  });

  int id{allocator->getId()};
  m_allocators_by_name[name] = allocator.get();
  if (name == "DEVICE") {
    m_allocators_by_name["DEVICE::0"] = allocator.get();
  }
  if (name.find("::0") != std::string::npos) {
    std::string base_name{name.substr(0, name.find("::") - 1)};
    m_allocators_by_name[base_name] = allocator.get();
  }
  if (name.find("::") == std::string::npos) {
    m_memory_resources[resource::string_to_resource(name)] = allocator.get();
  }
  m_allocators_by_id[id] = allocator.get();
  m_allocators.emplace_front(std::move(allocator));

  return Allocator{m_allocators_by_name[name]};
}

strategy::AllocationStrategy* ResourceManager::getAllocationStrategy(const std::string& name)
{
  resource::MemoryResourceRegistry& registry{resource::MemoryResourceRegistry::getInstance()};
  auto resource_names = registry.getResourceNames();

  UMPIRE_LOG(Debug, "(\"" << name << "\")");
  auto allocator = m_allocators_by_name.find(name);
  if (allocator == m_allocators_by_name.end()) {
    auto resource_name = std::find(resource_names.begin(), resource_names.end(), name);
    if (resource_name != std::end(resource_names)) {
      makeResource(name);
    } else {
      UMPIRE_ERROR(runtime_error, fmt::format("Allocator \"{}\" not found. Available allocators: {}", name,
                                              getAllocatorInformation()));
    }
  }

  return m_allocators_by_name[name];
}

std::optional<Allocator> ResourceManager::tryGetAllocator(const std::string& name)
{
  UMPIRE_LOG(Debug, "(\"" << name << "\")");

  resource::MemoryResourceRegistry& registry{resource::MemoryResourceRegistry::getInstance()};
  auto resource_names = registry.getResourceNames();

  auto allocator = m_allocators_by_name.find(name);
  if (allocator == m_allocators_by_name.end()) {
    auto resource_name = std::find(resource_names.begin(), resource_names.end(), name);
    if (resource_name != std::end(resource_names)) {
      makeResource(name);
      allocator = m_allocators_by_name.find(name);
    }
  }

  if (allocator == m_allocators_by_name.end()) {
    return std::nullopt;
  }

  return Allocator{allocator->second};
}

Allocator ResourceManager::getAllocator(const std::string& name)
{
  UMPIRE_LOG(Debug, "(\"" << name << "\")");
  return Allocator(getAllocationStrategy(name));
}

Allocator ResourceManager::getAllocator(const char* name)
{
  return getAllocator(std::string{name});
}

Allocator ResourceManager::getAllocator(resource::MemoryResourceType resource_type)
{
  UMPIRE_LOG(Debug, "(\"" << static_cast<std::size_t>(resource_type) << "\")");

  auto allocator = m_memory_resources.find(resource_type);
  if (allocator == m_memory_resources.end()) {
    return getAllocator(resource::resource_to_string(resource_type));
  } else {
    return Allocator(m_memory_resources[resource_type]);
  }
}

Allocator ResourceManager::getAllocator(int id)
{
  UMPIRE_LOG(Debug, "(\"" << id << "\")");

  if (id < 0) {
    UMPIRE_ERROR(runtime_error, fmt::format("Passed an invalid id: {}. Is this a DeviceAllocator instead?", id));
  }

  if (id == umpire::invalid_allocator_id) {
    UMPIRE_ERROR(runtime_error, "Passed umpire::invalid_allocator_id");
  }

  auto allocator = m_allocators_by_id.find(id);
  if (allocator == m_allocators_by_id.end()) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Allocator {} not found. Available allocators: {}", id, getAllocatorInformation()));
  }

  return Allocator(m_allocators_by_id[id]);
}

Allocator ResourceManager::getDefaultAllocator()
{
  UMPIRE_LOG(Debug, "");

  if (!m_default_allocator) {
    UMPIRE_LOG(Debug, "Initializing m_default_allocator as HOST");
    m_default_allocator = getAllocator("HOST").getAllocationStrategy();
  }

  return Allocator(m_default_allocator);
}

std::vector<std::string> ResourceManager::getResourceNames()
{
  resource::MemoryResourceRegistry& registry{resource::MemoryResourceRegistry::getInstance()};

  return registry.getResourceNames();
}

std::vector<std::string> ResourceManager::getSharedAllocatorNames()
{
  if (m_shared_allocator_names.size() == 0) {
    UMPIRE_LOG(Debug, "Called getSharedAllocatorNames, but there are none. Returning empty vector.");
    return std::vector<std::string>(); // Return an empty vector of strings
  }

  return m_shared_allocator_names;
}

void ResourceManager::setDefaultAllocator(Allocator allocator) noexcept
{
  UMPIRE_LOG(Debug, "(\"" << allocator.getName() << "\")");

  umpire::event::record([&](auto& event) {
    event.name("set_default_allocator")
        .category(event::category::operation)
        .arg("allocator_ref", (void*)allocator.getAllocationStrategy())
        .tag("allocator_name", allocator.getName())
        .tag("replay", "true");
  });

  m_default_allocator = allocator.getAllocationStrategy();
}

void ResourceManager::addAlias(const std::string& name, Allocator allocator)
{
  if (isAllocator(name)) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Allocator \"{}\" is already an alias for \"{}\"", name, getAllocator(name).getName()));
  }

  m_allocators_by_name[name] = allocator.getAllocationStrategy();
}

void ResourceManager::removeAlias(const std::string& name, Allocator allocator)
{
  if (!isAllocator(name)) {
    UMPIRE_ERROR(runtime_error, fmt::format("Allocator \"{}\" is not registered", name));
  }

  auto a = m_allocators_by_name.find(name);
  if (a->second->getName().compare(name) == 0) {
    UMPIRE_ERROR(runtime_error, fmt::format("\"{}\" is not an alias, so cannot be removed", name));
  }

  if (a->second->getId() != allocator.getId()) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("\"{}\" is not is not registered as an alias of {}", name, allocator.getName()));
  }

  m_allocators_by_name.erase(a);
}

bool ResourceManager::isBuiltinAllocator(strategy::AllocationStrategy* strategy)
{
  for (const auto& entry : m_memory_resources) {
    if (entry.second == strategy) {
      return true;
    }
  }

  std::string name = strategy->getName();
  if (name == "__umpire_internal_null" || name == "__umpire_internal_0_byte_pool") {
    return true;
  }

  return false;
}

void ResourceManager::destroyAllocator(const std::string& name, bool free_allocations)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  UMPIRE_LOG(Debug, "(name=\"" << name << "\", free_allocations=" << free_allocations << ")");

  auto it = m_allocators_by_name.find(name);
  if (it == m_allocators_by_name.end()) {
    UMPIRE_ERROR(runtime_error, fmt::format("Allocator \"{}\" not found", name));
  }

  strategy::AllocationStrategy* strategy = it->second;
  int id = strategy->getId();

  const std::string& strategy_name = strategy->getName();
  const bool is_shared_resource =
      (strategy_name == "SHARED") || (strategy_name.rfind("SHARED::", 0) == 0);

  if (isBuiltinAllocator(strategy) && !is_shared_resource) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Cannot destroy builtin allocator \"{}\"", name));
  }

  auto records = umpire::get_allocator_records(Allocator(strategy));

  if (isStrictDestructionMode()) {
    if (!records.empty() && !free_allocations) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Allocator \"{}\" has {} active allocations. "
                              "Use free_allocations=true or deallocate them first.",
                              name, records.size()));
    }
  } else if (!free_allocations && !records.empty()) {
    UMPIRE_LOG(Warning, "Allocator \"" << name << "\" may have active allocations. "
                        << "Destroying anyway (non-strict mode).");
  }


  if (isStrictDestructionMode()) {
    std::vector<std::string> child_names;
    for (const auto& alloc : m_allocators) {
      if (alloc.get() != strategy && alloc->getParent() == strategy) {
        child_names.push_back(alloc->getName());
      }
    }

    if (!child_names.empty()) {
      std::string children_str;
      for (size_t i = 0; i < child_names.size(); ++i) {
        if (i > 0) children_str += ", ";
        children_str += child_names[i];
      }

      UMPIRE_ERROR(runtime_error,
                   fmt::format("Allocator \"{}\" is a parent of other allocators: {}. "
                              "Destroy children first.",
                              name, children_str));
    }
  } else {
    UMPIRE_LOG(Warning, "Allocator \"" << name << "\" may be a parent of other allocators. "
                        << "Destroying anyway (non-strict mode).");
  }

  if (free_allocations) {
    UMPIRE_LOG(Debug, "Freeing " << records.size() << " allocations");
    Allocator allocator{strategy};
    for (const auto& record : records) {
      allocator.deallocate(record.ptr);
    }
  } else if (!records.empty()) {
    //
    // In non-strict mode, destroying an allocator with active allocations
    // intentionally "leaks" those allocations. Ensure we remove their records
    // so we don't retain dangling strategy pointers that could later collide
    // with a new allocator at the same address.
    //
    UMPIRE_LOG(Warning, "Untracking " << records.size() << " active allocations for allocator \"" << name
                                     << "\" (allocator destroyed without freeing allocations).");
    for (const auto& record : records) {
      deregisterAllocation(record.ptr);
    }
  }

  std::vector<std::string> names_to_remove;
  for (const auto& entry : m_allocators_by_name) {
    if (entry.second == strategy) {
      names_to_remove.push_back(entry.first);
    }
  }

  for (const auto& n : names_to_remove) {
    m_allocators_by_name.erase(n);
  }

  m_allocators_by_id.erase(id);

  for (auto it_mem = m_memory_resources.begin(); it_mem != m_memory_resources.end();) {
    if (it_mem->second == strategy) {
      it_mem = m_memory_resources.erase(it_mem);
    } else {
      ++it_mem;
    }
  }

  auto shared_it = std::find(m_shared_allocator_names.begin(), m_shared_allocator_names.end(), name);
  if (shared_it != m_shared_allocator_names.end()) {
    m_shared_allocator_names.erase(shared_it);
  }

  for (auto it_alloc = m_allocators.begin(); it_alloc != m_allocators.end(); ++it_alloc) {
    if (it_alloc->get() == strategy) {
      m_allocators.erase(it_alloc);
      break;
    }
  }

  umpire::event::record([&](auto& event) {
    event.name("destroy_allocator")
        .category(event::category::operation)
        .arg("allocator_name", name)
        .arg("allocator_id", id)
        .arg("freed_allocations", free_allocations)
        .tag("replay", "true");
  });

  UMPIRE_LOG(Debug, "Allocator \"" << name << "\" destroyed successfully");
}

void ResourceManager::destroyAllocator(int id, bool free_allocations)
{
  UMPIRE_LOG(Debug, "(id=" << id << ", free_allocations=" << free_allocations << ")");

  auto it = m_allocators_by_id.find(id);
  if (it == m_allocators_by_id.end()) {
    UMPIRE_ERROR(runtime_error, fmt::format("Allocator with id {} not found", id));
  }

  destroyAllocator(it->second->getName(), free_allocations);
}

Allocator ResourceManager::getAllocator(void* ptr)
{
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ")");
  return Allocator(findAllocatorForPointer(ptr));
}

bool ResourceManager::isAllocator(const std::string& name) noexcept
{
  resource::MemoryResourceRegistry& registry{resource::MemoryResourceRegistry::getInstance()};
  auto resource_names = registry.getResourceNames();

  return (m_allocators_by_name.find(name) != m_allocators_by_name.end() ||
          std::find(resource_names.begin(), resource_names.end(), name) != std::end(resource_names));
}

bool ResourceManager::isAllocator(int id) noexcept
{
  return (m_allocators_by_id.find(id) != m_allocators_by_id.end());
}

bool ResourceManager::hasAllocator(void* ptr)
{
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ")");

  return m_allocations.contains(ptr);
}

void ResourceManager::registerAllocation(void* ptr, util::AllocationRecord record)
{
  if (!ptr) {
    UMPIRE_ERROR(runtime_error, "Cannot register nullptr!");
  }

  UMPIRE_LOG(Debug,
             "(ptr=" << ptr << ", size=" << record.size << ", strategy=" << record.strategy << ") with " << this);

  UMPIRE_RECORD_BACKTRACE(record);

  m_allocations.insert(ptr, record);
}

util::AllocationRecord ResourceManager::deregisterAllocation(void* ptr)
{
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ")");
  return m_allocations.remove(ptr);
}

const util::AllocationRecord* ResourceManager::findAllocationRecord(void* ptr) const
{
  auto alloc_record = m_allocations.find(ptr);

  if (!alloc_record->strategy) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot find allocator for {}", ptr));
  }

  UMPIRE_LOG(Debug, "(Returning allocation record for ptr = " << ptr << ")");

  return alloc_record;
}

void ResourceManager::copy(void* dst_ptr, void* src_ptr, std::size_t size)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  auto src_alloc_record = m_allocations.find(src_ptr);
  std::ptrdiff_t src_offset = static_cast<char*>(src_ptr) - static_cast<char*>(src_alloc_record->ptr);

  if (size == 0) {
    size = src_alloc_record->size - src_offset;
  }

  auto dst_alloc_record = m_allocations.find(dst_ptr);
  std::ptrdiff_t dst_offset = static_cast<char*>(dst_ptr) - static_cast<char*>(dst_alloc_record->ptr);

  UMPIRE_LOG(Debug, "(src_ptr=" << src_ptr << ", dst_ptr=" << dst_ptr << ", size=" << size << ")");

  umpire::event::record([&](auto& event) {
    event.name("copy")
        .category(event::category::operation)
        .arg("src", src_ptr)
        .arg("dst", dst_ptr)
        .arg("src_offset", src_offset)
        .arg("dst_offset", dst_offset)
        .arg("size", size)
        .arg("src_allocator_ref", (void*)src_alloc_record->strategy)
        .arg("dst_allocator_ref", (void*)dst_alloc_record->strategy)
        .tag("src_allocator_name", src_alloc_record->strategy->getName())
        .tag("dst_allocator_name", dst_alloc_record->strategy->getName())
        .tag("replay", "true");
  });

  umpire::copy(static_cast<void*>(src_ptr), static_cast<void*>(dst_ptr), size);
#else
  UMPIRE_LOG(Debug, "(src_ptr=" << src_ptr << ", dst_ptr=" << dst_ptr << ", size=" << size << ")");

  auto& op_registry = op::MemoryOperationRegistry::getInstance();

  auto src_alloc_record = m_allocations.find(src_ptr);
  std::ptrdiff_t src_offset = static_cast<char*>(src_ptr) - static_cast<char*>(src_alloc_record->ptr);
  std::size_t src_size = src_alloc_record->size - src_offset;

  auto dst_alloc_record = m_allocations.find(dst_ptr);
  std::ptrdiff_t dst_offset = static_cast<char*>(dst_ptr) - static_cast<char*>(dst_alloc_record->ptr);
  std::size_t dst_size = dst_alloc_record->size - dst_offset;

  if (size == 0) {
    size = src_size;
  }

  umpire::event::record([&](auto& event) {
    event.name("copy")
        .category(event::category::operation)
        .arg("src", src_ptr)
        .arg("dst", dst_ptr)
        .arg("src_offset", src_offset)
        .arg("dst_offset", dst_offset)
        .arg("size", size)
        .arg("src_allocator_ref", (void*)src_alloc_record->strategy)
        .arg("dst_allocator_ref", (void*)dst_alloc_record->strategy)
        .tag("src_allocator_name", src_alloc_record->strategy->getName())
        .tag("dst_allocator_name", dst_alloc_record->strategy->getName())
        .tag("replay", "true");
  });

  if (size > dst_size) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("Not enough space in destination to copy {} bytes into {} bytes", size, dst_size));
  }

  auto op = op_registry.find("COPY", src_alloc_record->strategy, dst_alloc_record->strategy);

  op->transform(src_ptr, &dst_ptr, src_alloc_record, dst_alloc_record, size);
#endif
}

camp::resources::EventProxy<camp::resources::Resource> ResourceManager::copy(void* dst_ptr, void* src_ptr,
                                                                             camp::resources::Resource& ctx,
                                                                             std::size_t size)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  auto src_alloc_record = m_allocations.find(src_ptr);
  std::ptrdiff_t src_offset = static_cast<char*>(src_ptr) - static_cast<char*>(src_alloc_record->ptr);

  if (size == 0) {
    size = src_alloc_record->size - src_offset;
  }

  auto dst_alloc_record = m_allocations.find(dst_ptr);
  std::ptrdiff_t dst_offset = static_cast<char*>(dst_ptr) - static_cast<char*>(dst_alloc_record->ptr);

  UMPIRE_LOG(Debug, "(src_ptr=" << src_ptr << ", dst_ptr=" << dst_ptr << ", size=" << size << ")");

  umpire::event::record([&](auto& event) {
    event.name("copy")
        .category(event::category::operation)
        .arg("src", src_ptr)
        .arg("dst", dst_ptr)
        .arg("src_offset", src_offset)
        .arg("dst_offset", dst_offset)
        .arg("size", size)
        .arg("src_allocator_ref", (void*)src_alloc_record->strategy)
        .arg("dst_allocator_ref", (void*)dst_alloc_record->strategy)
        .tag("src_allocator_name", src_alloc_record->strategy->getName())
        .tag("dst_allocator_name", dst_alloc_record->strategy->getName())
        .tag("replay", "true")
        .tag("async", "true");
  });

  return umpire::copy(static_cast<void*>(src_ptr), static_cast<void*>(dst_ptr), size, ctx);
#else
  UMPIRE_LOG(Debug, "(src_ptr=" << src_ptr << ", dst_ptr=" << dst_ptr << ", size=" << size << ")");

  auto& op_registry = op::MemoryOperationRegistry::getInstance();

  auto src_alloc_record = m_allocations.find(src_ptr);
  std::ptrdiff_t src_offset = static_cast<char*>(src_ptr) - static_cast<char*>(src_alloc_record->ptr);
  std::size_t src_size = src_alloc_record->size - src_offset;

  auto dst_alloc_record = m_allocations.find(dst_ptr);
  std::ptrdiff_t dst_offset = static_cast<char*>(dst_ptr) - static_cast<char*>(dst_alloc_record->ptr);
  std::size_t dst_size = dst_alloc_record->size - dst_offset;

  if (size == 0) {
    size = src_size;
  }

  umpire::event::record([&](auto& event) {
    event.name("copy")
        .category(event::category::operation)
        .arg("src", src_ptr)
        .arg("dst", dst_ptr)
        .arg("src_offset", src_offset)
        .arg("dst_offset", dst_offset)
        .arg("size", size)
        .arg("src_allocator_ref", (void*)src_alloc_record->strategy)
        .arg("dst_allocator_ref", (void*)dst_alloc_record->strategy)
        .tag("src_allocator_name", src_alloc_record->strategy->getName())
        .tag("dst_allocator_name", dst_alloc_record->strategy->getName())
        .tag("replay", "true")
        .tag("async", "true");
  });

  if (size > dst_size) {
    UMPIRE_ERROR(runtime_error, fmt::format("Not enough resource in destination for copy: {} -> {}", size, dst_size));
  }

  auto op = op_registry.find("COPY", src_alloc_record->strategy, dst_alloc_record->strategy);

  return op->transform_async(src_ptr, &dst_ptr, src_alloc_record, dst_alloc_record, size, ctx);
#endif
}

void ResourceManager::memset(void* ptr, int value, std::size_t length)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  auto alloc_record = m_allocations.find(ptr);
  std::ptrdiff_t offset = static_cast<char*>(ptr) - static_cast<char*>(alloc_record->ptr);
  std::size_t size = alloc_record->size - offset;

  if (length == 0) {
    length = size;
  }
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ", value=" << value << ", length=" << length << ")");

  umpire::event::record([&](auto& event) {
    event.name("memset")
        .category(event::category::operation)
        .arg("ptr", ptr)
        .arg("value", value)
        .arg("size", size)
        .arg("allocator_ref", (void*)alloc_record->strategy)
        .tag("allocator_name", alloc_record->strategy->getName())
        .tag("replay", "true");
  });

  umpire::memset(static_cast<void*>(ptr), value, length);
#else
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ", value=" << value << ", length=" << length << ")");

  auto& op_registry = op::MemoryOperationRegistry::getInstance();

  auto alloc_record = m_allocations.find(ptr);

  std::ptrdiff_t offset = static_cast<char*>(ptr) - static_cast<char*>(alloc_record->ptr);
  std::size_t size = alloc_record->size - offset;

  if (length == 0) {
    length = size;
  }

  umpire::event::record([&](auto& event) {
    event.name("memset")
        .category(event::category::operation)
        .arg("ptr", ptr)
        .arg("value", value)
        .arg("size", size)
        .arg("allocator_ref", (void*)alloc_record->strategy)
        .tag("allocator_name", alloc_record->strategy->getName())
        .tag("replay", "true");
  });

  if (length > size) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot memset over the end of allocation: {} -> {}", length, size));
  }

  auto op = op_registry.find("MEMSET", alloc_record->strategy, alloc_record->strategy);

  op->apply(ptr, alloc_record, value, length);
#endif
}

camp::resources::EventProxy<camp::resources::Resource> ResourceManager::memset(void* ptr, int value,
                                                                               camp::resources::Resource& ctx,
                                                                               std::size_t length)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  auto alloc_record = m_allocations.find(ptr);
  std::ptrdiff_t offset = static_cast<char*>(ptr) - static_cast<char*>(alloc_record->ptr);
  std::size_t size = alloc_record->size - offset;

  if (length == 0) {
    length = size;
  }

  UMPIRE_LOG(Debug, "(ptr=" << ptr << ", value=" << value << ", length=" << length << ")");

  umpire::event::record([&](auto& event) {
    event.name("memset")
        .category(event::category::operation)
        .arg("ptr", ptr)
        .arg("value", value)
        .arg("size", size)
        .arg("allocator_ref", (void*)alloc_record->strategy)
        .tag("allocator_name", alloc_record->strategy->getName())
        .tag("replay", "true")
        .tag("async", "true");
  });

  return umpire::memset(static_cast<void*>(ptr), value, length, ctx);
#else
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ", value=" << value << ", length=" << length << ")");

  auto& op_registry = op::MemoryOperationRegistry::getInstance();

  auto alloc_record = m_allocations.find(ptr);

  std::ptrdiff_t offset = static_cast<char*>(ptr) - static_cast<char*>(alloc_record->ptr);
  std::size_t size = alloc_record->size - offset;

  if (length == 0) {
    length = size;
  }

  umpire::event::record([&](auto& event) {
    event.name("memset")
        .category(event::category::operation)
        .arg("ptr", ptr)
        .arg("value", value)
        .arg("size", size)
        .arg("allocator_ref", (void*)alloc_record->strategy)
        .tag("allocator_name", alloc_record->strategy->getName())
        .tag("replay", "true")
        .tag("async", "true");
  });

  if (length > size) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot memset over the end of allocation: {} -> {}", length, size));
  }

  auto op = op_registry.find("MEMSET", alloc_record->strategy, alloc_record->strategy);

  return op->apply_async(ptr, alloc_record, value, length, ctx);
#endif
}

void* ResourceManager::reallocate(void* current_ptr, std::size_t new_size)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  strategy::AllocationStrategy* strategy;

  if (current_ptr != nullptr) {
    auto alloc_record = m_allocations.find(current_ptr);
    strategy = alloc_record->strategy;
  } else {
    strategy = getDefaultAllocator().getAllocationStrategy();
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", current_ptr)
        .arg("size", new_size)
        .arg("allocator_ref", (void*)strategy)
        .tag("allocator_name", strategy->getName())
        .tag("replay", "true");
  });

  void* new_ptr;

  if (!current_ptr) {
    new_ptr = Allocator(strategy).allocate(new_size);
  } else if (new_size == 0) {
    Allocator alloc(strategy);
    alloc.deallocate(current_ptr);
    new_ptr = alloc.allocate(new_size);
  } else {
    UMPIRE_LOG(Debug, "(current_ptr=" << current_ptr << ", new_size=" << new_size << ")");
    new_ptr = umpire::reallocate(&current_ptr, new_size);
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("allocator_ref", (void*)strategy)
        .tag("allocator_name", strategy->getName())
        .arg("new_ptr", new_ptr);
  });

  return new_ptr;
#else
  strategy::AllocationStrategy* strategy;

  if (current_ptr != nullptr) {
    auto alloc_record = m_allocations.find(current_ptr);
    strategy = alloc_record->strategy;
  } else {
    strategy = getDefaultAllocator().getAllocationStrategy();
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", current_ptr)
        .arg("size", new_size)
        .arg("allocator_ref", (void*)strategy)
        .tag("allocator_name", strategy->getName())
        .tag("replay", "true");
  });

  void* new_ptr{reallocate_impl(current_ptr, new_size, Allocator(strategy))};

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("allocator_ref", (void*)strategy)
        .tag("allocator_name", strategy->getName())
        .arg("new_ptr", new_ptr);
  });

  return new_ptr;
#endif
}

void* ResourceManager::reallocate(void* current_ptr, std::size_t new_size, camp::resources::Resource& ctx)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  strategy::AllocationStrategy* strategy;

  if (current_ptr != nullptr) {
    auto alloc_record = m_allocations.find(current_ptr);
    strategy = alloc_record->strategy;
  } else {
    strategy = getDefaultAllocator().getAllocationStrategy();
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", current_ptr)
        .arg("size", new_size)
        .arg("allocator_ref", (void*)strategy)
        .tag("allocator_name", strategy->getName())
        .tag("replay", "true")
        .tag("async", "true");
  });

  void* new_ptr;

  if (!current_ptr) {
    new_ptr = Allocator(strategy).allocate(new_size);
  } else if (new_size == 0) {
    Allocator alloc(strategy);
    alloc.deallocate(current_ptr);
    new_ptr = alloc.allocate(new_size);
  } else {
    UMPIRE_LOG(Debug, "(current_ptr=" << current_ptr << ", new_size=" << new_size << ")");
    auto ev = umpire::reallocate(&current_ptr, new_size, ctx);
    UMPIRE_USE_VAR(ev);
    new_ptr = current_ptr;
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("new_ptr", new_ptr)
        .arg("allocator_ref", (void*)strategy)
        .tag("allocator_name", strategy->getName());
  });

  return new_ptr;
#else
  strategy::AllocationStrategy* strategy;

  if (current_ptr != nullptr) {
    auto alloc_record = m_allocations.find(current_ptr);
    strategy = alloc_record->strategy;
  } else {
    strategy = getDefaultAllocator().getAllocationStrategy();
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", current_ptr)
        .arg("size", new_size)
        .arg("allocator_ref", (void*)strategy)
        .tag("allocator_name", strategy->getName())
        .tag("replay", "true")
        .tag("async", "true");
  });

  void* new_ptr{reallocate_impl(current_ptr, new_size, Allocator(strategy), ctx)};

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("new_ptr", new_ptr)
        .arg("allocator_ref", (void*)strategy)
        .tag("allocator_name", strategy->getName());
  });

  return new_ptr;
#endif
}

void* ResourceManager::reallocate(void* current_ptr, std::size_t new_size, Allocator alloc)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  if (current_ptr) {
    // We need to check if the current pointer belongs to the same allocator
    auto src_allocator = getAllocator(current_ptr);
    if (src_allocator.getId() != alloc.getId()) {
      UMPIRE_ERROR(runtime_error, fmt::format("Cannot reallocate {} from allocator \"{}\" with allocator \"{}\"",
                                              current_ptr, src_allocator.getName(), alloc.getName()));
    }
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", current_ptr)
        .arg("size", new_size)
        .arg("allocator_ref", (void*)alloc.getAllocationStrategy())
        .tag("allocator_name", alloc.getName())
        .tag("replay", "true");
  });

  void* new_ptr;

  if (!current_ptr) {
    new_ptr = alloc.allocate(new_size);
  } else if (new_size == 0) {
    auto alloc_record = m_allocations.find(current_ptr);
    auto a = Allocator(alloc_record->strategy);
    a.deallocate(current_ptr);
    new_ptr = alloc.allocate(new_size);
  } else {
    UMPIRE_LOG(Debug, "(current_ptr=" << current_ptr << ", new_size=" << new_size << ", with Allocator "
                                      << alloc.getName() << ")");
    new_ptr = umpire::reallocate(&current_ptr, new_size);
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("new_ptr", new_ptr)
        .arg("allocator_ref", (void*)alloc.getAllocationStrategy())
        .tag("allocator_name", alloc.getName());
  });

  return new_ptr;
#else
  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", current_ptr)
        .arg("size", new_size)
        .arg("allocator_ref", (void*)alloc.getAllocationStrategy())
        .tag("allocator_name", alloc.getName())
        .tag("replay", "true");
  });

  void* new_ptr{reallocate_impl(current_ptr, new_size, alloc)};

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("new_ptr", new_ptr)
        .arg("allocator_ref", (void*)alloc.getAllocationStrategy())
        .tag("allocator_name", alloc.getName());
  });

  return new_ptr;
#endif
}

void* ResourceManager::reallocate(void* current_ptr, std::size_t new_size, Allocator alloc,
                                  camp::resources::Resource& ctx)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  if (current_ptr) {
    // We need to check if the current pointer belongs to the same allocator
    auto src_allocator = getAllocator(current_ptr);
    if (src_allocator.getId() != alloc.getId()) {
      UMPIRE_ERROR(runtime_error, fmt::format("Cannot reallocate {} from allocator \"{}\" with allocator \"{}\"",
                                              current_ptr, src_allocator.getName(), alloc.getName()));
    }
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", current_ptr)
        .arg("size", new_size)
        .arg("allocator_ref", (void*)alloc.getAllocationStrategy())
        .tag("allocator_name", alloc.getName())
        .tag("replay", "true")
        .tag("async", "true");
  });

  void* new_ptr;

  if (!current_ptr) {
    new_ptr = alloc.allocate(new_size);
  } else if (new_size == 0) {
    auto alloc_record = m_allocations.find(current_ptr);
    auto a = Allocator(alloc_record->strategy);
    a.deallocate(current_ptr);
    new_ptr = alloc.allocate(new_size);
  } else {
    UMPIRE_LOG(Debug, "(current_ptr=" << current_ptr << ", new_size=" << new_size << ", with Allocator "
                                      << alloc.getName() << ")");
    auto ev = umpire::reallocate(&current_ptr, new_size, ctx);
    UMPIRE_USE_VAR(ev);
    new_ptr = current_ptr;
  }

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("new_ptr", new_ptr)
        .arg("allocator_ref", (void*)alloc.getAllocationStrategy())
        .tag("allocator_name", alloc.getName());
  });

  return new_ptr;
#else
  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("current_ptr", current_ptr)
        .arg("size", new_size)
        .arg("allocator_ref", (void*)alloc.getAllocationStrategy())
        .tag("allocator_name", alloc.getName())
        .tag("replay", "true")
        .tag("async", "true");
  });

  void* new_ptr{reallocate_impl(current_ptr, new_size, alloc, ctx)};

  umpire::event::record([&](auto& event) {
    event.name("reallocate")
        .category(event::category::operation)
        .arg("new_ptr", new_ptr)
        .arg("allocator_ref", (void*)alloc.getAllocationStrategy())
        .tag("allocator_name", alloc.getName());
  });

  return new_ptr;
#endif
}

void* ResourceManager::reallocate_impl(void* current_ptr, std::size_t new_size, Allocator allocator)
{
  UMPIRE_LOG(Debug, "(current_ptr=" << current_ptr << ", new_size=" << new_size << ", with Allocator "
                                    << allocator.getName() << ")");

  void* new_ptr;

  //
  // If this is a brand new allocation, no reallocation necessary, just allocate
  //
  if (current_ptr == nullptr) {
    new_ptr = allocator.allocate(new_size);
  } else {
    auto alloc_record = m_allocations.find(current_ptr);
    auto alloc = Allocator(alloc_record->strategy);

    if (alloc_record->strategy != allocator.getAllocationStrategy()) {
      UMPIRE_ERROR(runtime_error, fmt::format("Cannot reallocate {} from allocator \"{}\" with allocator \"{}\"",
                                              current_ptr, alloc.getName(), allocator.getName()));
    }

    //
    // Special case 0-byte size here
    //
    if (new_size == 0) {
      alloc.deallocate(current_ptr);
      new_ptr = alloc.allocate(new_size);
    } else {
      auto& op_registry = op::MemoryOperationRegistry::getInstance();

      if (current_ptr != alloc_record->ptr) {
        UMPIRE_ERROR(runtime_error,
                     fmt::format("Cannot reallocate an offset ptr (ptr={}, base={})", current_ptr, alloc_record->ptr));
      }

      std::shared_ptr<umpire::op::MemoryOperation> op;
      if (alloc_record->strategy->getPlatform() == Platform::host &&
          getAllocator("HOST").getId() != alloc_record->strategy->getId()) {
        op = op_registry.find("REALLOCATE", std::make_pair(Platform::undefined, Platform::undefined));
      } else {
        op = op_registry.find("REALLOCATE", alloc_record->strategy, alloc_record->strategy);
      }

      op->transform(current_ptr, &new_ptr, alloc_record, alloc_record, new_size);
    }
  }

  return new_ptr;
}

void* ResourceManager::reallocate_impl(void* current_ptr, std::size_t new_size, Allocator allocator,
                                       camp::resources::Resource& ctx)
{
  UMPIRE_LOG(Debug, "(current_ptr=" << current_ptr << ", new_size=" << new_size << ", with Allocator "
                                    << allocator.getName() << ")");

  void* new_ptr;

  //
  // If this is a brand new allocation, no reallocation necessary, just allocate
  //
  if (current_ptr == nullptr) {
    new_ptr = allocator.allocate(new_size);
  } else {
    auto alloc_record = m_allocations.find(current_ptr);
    auto alloc = Allocator(alloc_record->strategy);

    if (alloc_record->strategy != allocator.getAllocationStrategy()) {
      UMPIRE_ERROR(runtime_error, fmt::format("Cannot reallocate {} from allocator \"{}\" with allocator \"{}\"",
                                              current_ptr, alloc.getName(), allocator.getName()));
    }

    //
    // Special case 0-byte size here
    //
    if (new_size == 0) {
      alloc.deallocate(current_ptr);
      new_ptr = alloc.allocate(new_size);
    } else {
      auto& op_registry = op::MemoryOperationRegistry::getInstance();

      if (current_ptr != alloc_record->ptr) {
        UMPIRE_ERROR(runtime_error,
                     fmt::format("Cannot reallocate an offset ptr (ptr={}, base={})", current_ptr, alloc_record->ptr));
      }

      std::shared_ptr<umpire::op::MemoryOperation> op;
      if (alloc_record->strategy->getPlatform() == Platform::host &&
          getAllocator("HOST").getId() != alloc_record->strategy->getId()) {
        op = op_registry.find("REALLOCATE", std::make_pair(Platform::undefined, Platform::undefined));
        op->transform(current_ptr, &new_ptr, alloc_record, alloc_record, new_size);
      } else {
        op = op_registry.find("REALLOCATE", alloc_record->strategy, alloc_record->strategy);
        op->transform_async(current_ptr, &new_ptr, alloc_record, alloc_record, new_size, ctx);
      }
    }
  }

  return new_ptr;
}

void* ResourceManager::move(void* src_ptr, Allocator allocator)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  UMPIRE_LOG(Debug, "(src_ptr=" << src_ptr << ", allocator=" << allocator.getName() << ")");

  auto alloc_record = m_allocations.find(src_ptr);

  // short-circuit if ptr was allocated by 'allocator'
  if (alloc_record->strategy == allocator.getAllocationStrategy()) {
    umpire::event::record([&](auto& event) {
      event.name("move")
          .category(event::category::operation)
          .arg("ptr", src_ptr)
          .arg("allocator_ref", (void*)allocator.getAllocationStrategy())
          .tag("allocator_name", allocator.getName())
          .tag("replay", "true");
    });
    return src_ptr;
  }

#if defined(UMPIRE_ENABLE_NUMA)
  {
    auto base_strategy = util::unwrap_allocator<strategy::AllocationStrategy>(allocator);

    // If found, use op::NumaMoveOperation to move in-place (same address
    // returned)
    if (dynamic_cast<strategy::NumaPolicy*>(base_strategy)) {
      auto& op_registry = op::MemoryOperationRegistry::getInstance();

      auto src_alloc_record = m_allocations.find(src_ptr);

      const std::size_t size{src_alloc_record->size};
      util::AllocationRecord dst_alloc_record{nullptr, size, allocator.getAllocationStrategy()};

      if (size > 0) {
        auto op = op_registry.find("MOVE", src_alloc_record->strategy, dst_alloc_record.strategy);
        void* ret{nullptr};
        op->transform(src_ptr, &ret, src_alloc_record, &dst_alloc_record, size);
        UMPIRE_ASSERT(ret == src_ptr);
      }

      umpire::event::record([&](auto& event) {
        event.name("move")
            .category(event::category::operation)
            .arg("ptr", src_ptr)
            .arg("allocator_ref", (void*)allocator.getAllocationStrategy())
            .tag("allocator_name", allocator.getName())
            .tag("replay", "true")
            .arg("result", src_ptr);
      });
      return src_ptr;
    }
  }
#endif

  // Check for offset pointers
  if (src_ptr != alloc_record->ptr) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot move an offset ptr (ptr={}, base={})", src_ptr, alloc_record->ptr));
  }

  // Allocate new memory in destination
  std::size_t size = getSize(src_ptr);
  void* dst_ptr = allocator.allocate(size);

  // Copy data
  umpire::copy(src_ptr, dst_ptr, size);

  // Deallocate original
  deallocate(src_ptr);

  umpire::event::record([&](auto& event) {
    event.name("move")
        .category(event::category::operation)
        .arg("ptr", src_ptr)
        .arg("allocator_ref", (void*)allocator.getAllocationStrategy())
        .tag("allocator_name", allocator.getName())
        .tag("replay", "true")
        .arg("result", dst_ptr);
  });

  return dst_ptr;
#else
  UMPIRE_LOG(Debug, "(src_ptr=" << src_ptr << ", allocator=" << allocator.getName() << ")");

  auto alloc_record = m_allocations.find(src_ptr);

  // short-circuit if ptr was allocated by 'allocator'
  if (alloc_record->strategy == allocator.getAllocationStrategy()) {
    umpire::event::record([&](auto& event) {
      event.name("move")
          .category(event::category::operation)
          .arg("ptr", src_ptr)
          .arg("allocator_ref", (void*)allocator.getAllocationStrategy())
          .tag("allocator_name", allocator.getName())
          .tag("replay", "true");
    });
    return src_ptr;
  }

#if defined(UMPIRE_ENABLE_NUMA)
  {
    auto base_strategy = util::unwrap_allocator<strategy::AllocationStrategy>(allocator);

    // If found, use op::NumaMoveOperation to move in-place (same address
    // returned)
    if (dynamic_cast<strategy::NumaPolicy*>(base_strategy)) {
      auto& op_registry = op::MemoryOperationRegistry::getInstance();

      auto src_alloc_record = m_allocations.find(src_ptr);

      const std::size_t size{src_alloc_record->size};
      util::AllocationRecord dst_alloc_record{nullptr, size, allocator.getAllocationStrategy()};

      if (size > 0) {
        auto op = op_registry.find("MOVE", src_alloc_record->strategy, dst_alloc_record.strategy);
        void* ret{nullptr};
        op->transform(src_ptr, &ret, src_alloc_record, &dst_alloc_record, size);
        UMPIRE_ASSERT(ret == src_ptr);
      }

      umpire::event::record([&](auto& event) {
        event.name("move")
            .category(event::category::operation)
            .arg("ptr", src_ptr)
            .arg("allocator_ref", (void*)allocator.getAllocationStrategy())
            .tag("allocator_name", allocator.getName())
            .tag("replay", "true")
            .arg("result", src_ptr);
      });
      return src_ptr;
    }
  }
#endif

  if (src_ptr != alloc_record->ptr) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot move an offset ptr (ptr={}, base={})", src_ptr, alloc_record->ptr));
  }

  void* dst_ptr{allocator.allocate(alloc_record->size)};
  copy(dst_ptr, src_ptr);

  deallocate(src_ptr);

  umpire::event::record([&](auto& event) {
    event.name("move")
        .category(event::category::operation)
        .arg("ptr", src_ptr)
        .arg("allocator_ref", (void*)allocator.getAllocationStrategy())
        .tag("allocator_name", allocator.getName())
        .tag("replay", "true")
        .arg("result", dst_ptr);
  });

  return dst_ptr;
#endif
}

camp::resources::EventProxy<camp::resources::Resource> ResourceManager::prefetch(void* ptr, int device,
                                                                                 camp::resources::Resource& ctx)
{
#if defined(UMPIRE_RM_USE_NEW_OPS)
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ", device=" << device << ")");

  auto alloc_record = m_allocations.find(ptr);

  if (alloc_record->strategy->getTraits().resource != umpire::MemoryResourceTraits::resource_type::um) {
    UMPIRE_ERROR(runtime_error, "ResourceManager::prefetch only works on allocations from a UM resource.");
  }

  // Get size from offset to end of allocation
  std::ptrdiff_t offset = static_cast<char*>(ptr) - static_cast<char*>(alloc_record->ptr);
  std::size_t size = alloc_record->size - offset;

  return umpire::prefetch(ptr, device, size, ctx);
#else
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ", device=" << device << ")");

  auto alloc_record = m_allocations.find(ptr);

  if (alloc_record->strategy->getTraits().resource != umpire::MemoryResourceTraits::resource_type::um) {
    UMPIRE_ERROR(runtime_error, "ResourceManager::prefetch only works on allocations from a UM resource.");
  }

  std::ptrdiff_t offset = static_cast<char*>(ptr) - static_cast<char*>(alloc_record->ptr);
  std::size_t size = alloc_record->size - offset;

  auto& op_registry = op::MemoryOperationRegistry::getInstance();
  auto op = op_registry.find("PREFETCH", alloc_record->strategy, alloc_record->strategy);
  return op->apply_async(ptr, alloc_record, device, size, ctx);
#endif
}

void ResourceManager::deallocate(void* ptr)
{
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ")");
  Allocator allocator{findAllocatorForPointer(ptr)};

  allocator.deallocate(ptr);
}

std::size_t ResourceManager::getSize(void* ptr) const
{
  auto record = m_allocations.find(ptr);
  UMPIRE_LOG(Debug, "(ptr=" << ptr << ") returning " << record->size);
  return record->size;
}

std::size_t ResourceManager::getInternalMemoryUsage() const
{
  return m_allocations.internalMemoryUsage();
}

strategy::AllocationStrategy* ResourceManager::findAllocatorForId(int id)
{
  auto allocator_i = m_allocators_by_id.find(id);

  if (allocator_i == m_allocators_by_id.end()) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot find allocator with id: {}", id));
  }

  UMPIRE_LOG(Debug, "(id=" << id << ") returning " << allocator_i->second);
  return allocator_i->second;
}

strategy::AllocationStrategy* ResourceManager::findAllocatorForPointer(void* ptr)
{
  auto allocation_record = m_allocations.find(ptr);

  if (!allocation_record->strategy) {
    UMPIRE_ERROR(runtime_error, fmt::format("Cannot find allocator for pointer: {}", ptr));
  }

  UMPIRE_LOG(Debug, "(ptr=" << ptr << ") returning " << allocation_record->strategy);
  return allocation_record->strategy;
}

std::vector<std::string> ResourceManager::getAllocatorNames() const noexcept
{
  std::vector<std::string> names;
  for (auto it = m_allocators_by_name.begin(); it != m_allocators_by_name.end(); ++it) {
    names.push_back(it->first);
  }

  UMPIRE_LOG(Debug, "() returning " << names.size() << " allocators");
  return names;
}

std::vector<int> ResourceManager::getAllocatorIds() const noexcept
{
  std::vector<int> ids;
  for (auto& it : m_allocators_by_id) {
    ids.push_back(it.first);
  }

  return ids;
}

int ResourceManager::getNextId() noexcept
{
  return m_id++;
}

std::string ResourceManager::getAllocatorInformation() const noexcept
{
  std::ostringstream info;
  std::unordered_set<std::string> seen_names;
  bool has_names{false};

  const auto append_name = [&](const std::string& name) {
    if (name == s_null_resource_name || name == s_zero_byte_pool_name) {
      return;
    }

    if (seen_names.insert(name).second) {
      info << "\n  - " << name;
      has_names = true;
    }
  };

  for (const auto& name : resource::MemoryResourceRegistry::getInstance().getResourceNames()) {
    append_name(name);
  }

  std::vector<std::string> extra_names;
  extra_names.reserve(m_allocators_by_name.size());

  for (const auto& it : m_allocators_by_name) {
    extra_names.push_back(it.first);
  }

  std::sort(extra_names.begin(), extra_names.end());
  for (const auto& name : extra_names) {
    append_name(name);
  }

  if (!has_names) {
    info << " (none)";
  }

  return info.str();
}

strategy::AllocationStrategy* ResourceManager::getZeroByteAllocator()
{
  return m_zero_byte_pool;
}

std::shared_ptr<op::MemoryOperation> ResourceManager::getOperation(const std::string& operation_name,
                                                                   Allocator src_allocator, Allocator dst_allocator)
{
  auto& op_registry = op::MemoryOperationRegistry::getInstance();

  return op_registry.find(operation_name, src_allocator.getAllocationStrategy(), dst_allocator.getAllocationStrategy());
}

bool ResourceManager::isStrictDestructionMode() const noexcept
{
  static const char* env_value = std::getenv("UMPIRE_STRICT_DESTRUCTION");
  return (env_value != nullptr);
}

int ResourceManager::getNumDevices() const
{
  int device_count{0};
#if defined(UMPIRE_ENABLE_CUDA)
  cudaError_t err = ::cudaGetDeviceCount(&device_count);
  if (err != cudaSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("cudaGetDeviceCount failed with error: {}", cudaGetErrorString(err)));
  }
#elif defined(UMPIRE_ENABLE_HIP)
  hipError_t err = hipGetDeviceCount(&device_count);
  if (err != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipGetDeviceCount failed with error: {}", hipGetErrorString(err)));
  }
#elif defined(UMPIRE_ENABLE_SYCL)
  sycl::queue queue{sycl::gpu_selector_v};
  sycl::platform platform = queue.get_device().get_platform();

  auto devices = platform.get_devices();
  for (auto& device : devices) {
    if (device.is_gpu()) {
      if (device.get_info<sycl::info::device::partition_max_sub_devices>() > 0) {
        device_count += device.get_info<sycl::info::device::partition_max_sub_devices>();
      } else {
        device_count++;
      }
    }
  }
#endif
  return device_count;
}

} // end of namespace umpire
