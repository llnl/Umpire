//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_ResourceManager_HPP
#define UMPIRE_ResourceManager_HPP

#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "camp/resource.hpp"
#include "umpire/Allocator.hpp"
#include "umpire/Tracking.hpp"
#include "umpire/resource/MemoryResourceTypes.hpp"
#include "umpire/strategy/AllocationStrategy.hpp"
#include "umpire/util/AllocationMap.hpp"

namespace umpire {

namespace op {
class MemoryOperation;
}

namespace strategy {
class ZeroByteHandler;

namespace mixins {
class AllocateNull;
}
} // namespace strategy

/*!
 * \brief
 */
class ResourceManager {
 public:
  /*!
   * \brief
   */
  static ResourceManager& getInstance();

  /*!
   * \brief Initialize the ResourceManager.
   *
   * This will create all registered MemoryResource objects
   */
  void initialize();

  /*!
   * \brief Get the names of all available Allocator objects.
   */
  std::vector<std::string> getAllocatorNames() const noexcept;

  /*!
   * \brief Get the ids of all available Allocator objects.
   */
  std::vector<int> getAllocatorIds() const noexcept;

  /*!
   * \brief Get the Allocator with the given name.
   */
  Allocator getAllocator(const std::string& name);

  /*!
   * \brief Try to get the Allocator with the given name.
   *
   * This function returns an empty optional if the allocator does not exist
   * or cannot be created, instead of throwing an exception.
   */
  std::optional<Allocator> tryGetAllocator(const std::string& name);

  Allocator getAllocator(const char* name);

  /*!
   * \brief Get the default Allocator for the given resource_type.
   */
  Allocator getAllocator(resource::MemoryResourceType resource_type);

  /*!
   * \brief Get the Allocator with the given ID.
   */
  Allocator getAllocator(int id);

  /*!
   * \brief Get the default Allocator.
   *
   * The default Allocator is used whenever an Allocator is required and one
   * is not provided, or cannot be inferred.
   *
   * \return The default Allocator.
   */
  Allocator getDefaultAllocator();

  /*!
   * \brief Get the names for existing Resources.
   *
   * The Memory Resource Registry dynamically populates available memory resource
   * types based on what's available. This function returns those names so they
   * can be used to determine allocator accessibility.
   *
   * \return The available resource names.
   */
  std::vector<std::string> getResourceNames();

  /*!
   * \brief Get the names for existing SHARED Allocator names, if any.
   *
   * The SHARED memory resource only indicates whether or not these SHARED allocators
   * exist. Since SHARED allocators are made at runtime, this function will actually
   * find the specific name of each SHARED allocator and return it.
   *
   * \return A vector of strings with the available SHARED allocator names.
   */
  std::vector<std::string> getSharedAllocatorNames();

  /*!
   * \brief Set the default Allocator.
   *
   * The default Allocator is used whenever an Allocator is required and one
   * is not provided, or cannot be inferred.
   *
   * \param allocator The Allocator to use as the default.
   */
  void setDefaultAllocator(Allocator allocator) noexcept;

  /*!
   * \brief Construct a new Allocator.
   */
  template <typename Strategy, bool introspection = true, typename... Args>
  Allocator makeAllocator(const std::string& name, Args&&... args);

  template <typename Strategy, typename... Args>
  Allocator makeAllocator(const std::string& name, Tracking tracked, Args&&... args);

  Allocator makeResource(const std::string& name);

  Allocator makeResource(const std::string& name, MemoryResourceTraits traits);

  /*!
   * \brief Add an Allocator alias.
   *
   * After this call, allocator can be retrieved by calling getAllocator(name).
   *
   * The same Allocator can have multiple aliases.
   *
   * \param name Name to alias Allocator with.
   * \param allocator Allocator to register.
   */
  void addAlias(const std::string& name, Allocator allocator);

  /*!
   * \brief Remove an Allocator alias.
   *
   * After calling, allocator can no longer be accessed by calling
   * getAllocator(name). If allocator is not registered under name, an error
   * will be thrown.
   *
   * If one of the default resource names (e.g. HOST) is used, an error will be
   * thrown.
   *
   * \param name Name to deregister Allocator with.
   * \param allocator Allocator to deregister.
   */
  void removeAlias(const std::string& name, Allocator allocator);

  /*!
   * \brief Destroy an allocator by name.
   *
   * Removes the allocator from the ResourceManager and frees associated
   * resources. Core resource allocators (HOST, DEVICE, etc.) cannot be
   * destroyed.
   *
   * Behavior is controlled by the UMPIRE_STRICT_DESTRUCTION environment variable:
   *
   * If UMPIRE_STRICT_DESTRUCTION is set (to any value):
   * - Throws error if allocator has active allocations and free_allocations=false
   * - Throws error if allocator is a parent of other allocators
   *
   * If UMPIRE_STRICT_DESTRUCTION is not set (default):
   * - Logs warning but proceeds if allocator has active allocations
   * - Logs warning but proceeds if allocator is a parent
   *
   * Example:
   *   export UMPIRE_STRICT_DESTRUCTION=1  // Enable strict mode
   *   unset UMPIRE_STRICT_DESTRUCTION     // Disable strict mode (default)
   *
   * \param name Name of the allocator to destroy
   * \param free_allocations If true, deallocates all active allocations
   *                         before destroying. Defaults to false.
   *
   * \throw runtime_error if allocator is a core resource or not found
   */
  void destroyAllocator(const std::string& name, bool free_allocations = false);

  /*!
   * \brief Destroy an allocator by ID.
   *
   * See destroyAllocator(const std::string&, bool) for detailed behavior.
   *
   * \param id ID of the allocator to destroy
   * \param free_allocations If true, deallocates all active allocations
   *                         before destroying. Defaults to false.
   *
   * \throw runtime_error if allocator is a core resource or not found
   */
  void destroyAllocator(int id, bool free_allocations = false);

  /*!
   * \brief Get the Allocator used to allocate ptr.
   *
   * \param ptr Pointer to find the Allocator for.
   * \return Allocator for the given ptr.
   */
  Allocator getAllocator(void* ptr);

  bool isAllocator(const std::string& name) noexcept;

  bool isAllocator(int id) noexcept;

  /*!
   * \brief Does the given pointer have an associated Allocator.
   *
   * \return True if the pointer has an associated Allocator.
   */
  bool hasAllocator(void* ptr);

  /*!
   * \brief register an allocation with the manager.
   */
  void registerAllocation(void* ptr, util::AllocationRecord record);

  /*!
   * \brief de-register the address ptr with the manager.
   *
   * \return the allocation record removed from the manager.
   */
  util::AllocationRecord deregisterAllocation(void* ptr);

  /*!
   * \brief Find the allocation record associated with an address ptr.
   *
   * \return the record if found, or throws an exception if not found.
   */
  const util::AllocationRecord* findAllocationRecord(void* ptr) const;

  /*!
   * \brief Copy size bytes of data from src_ptr to dst_ptr.
   *
   * Both the src_ptr and dst_ptr addresses must be allocated by Umpire. They
   * can be offset from any Umpire-managed base address.
   *
   * The dst_ptr must be large enough to accommodate size bytes of data.
   *
   * \param dst_ptr Destination pointer.
   * \param src_ptr Source pointer.
   * \param size Size in bytes.
   */
  void copy(void* dst_ptr, void* src_ptr, std::size_t size = 0);

  camp::resources::EventProxy<camp::resources::Resource> copy(void* dst_ptr, void* src_ptr,
                                                              camp::resources::Resource& ctx, std::size_t size = 0);

  /*!
   * \brief Set the first length bytes of ptr to the value val.
   *
   * \param ptr Pointer to data.
   * \param val Value to set.
   * \param length Number of bytes to set to val.
   */
  void memset(void* ptr, int val, std::size_t length = 0);

  camp::resources::EventProxy<camp::resources::Resource> memset(void* ptr, int val, camp::resources::Resource& ctx,
                                                                std::size_t length = 0);

  /*!
   * \brief Reallocate current_ptr to new_size.
   *
   * \param current_ptr Source pointer to reallocate.
   * \param new_size New size of pointer.
   *
   * If current_ptr is nullptr, then the default allocator will be used to
   * allocate data. The default allocator may be set with a call to
   * setDefaultAllocator(Allocator allocator).
   *
   * NOTE 1: This is not thread safe
   * NOTE 2: If the allocator for which current_ptr is intended is different
   *         from the default allocator, then all subsequent reallocate calls
   *         will result in allocations from the default allocator which may
   *         not be the intended behavior.
   *
   * If new_size is 0, then the current_ptr will be deallocated if it is not
   * a nullptr, and a zero-byte allocation will be returned.
   *
   * \return Reallocated pointer.
   *
   */
  void* reallocate(void* current_ptr, std::size_t new_size);

  void* reallocate(void* current_ptr, std::size_t new_size, camp::resources::Resource& ctx);

  /*!
   * \brief Reallocate current_ptr to new_size.
   *
   * \param current_ptr Source pointer to reallocate.
   * \param new_size New size of pointer.
   * \param allocator Allocator to use if current_ptr is null.
   *
   * If current_ptr is null, then allocator will be used to allocate the
   * data.
   *
   * If new_size is 0, then the current_ptr will be deallocated if it is not
   * a nullptr, and a zero-byte allocation will be returned.
   *
   * \return Reallocated pointer.
   *
   */
  void* reallocate(void* current_ptr, std::size_t new_size, Allocator allocator);

  void* reallocate(void* current_ptr, std::size_t new_size, Allocator allocator, camp::resources::Resource& ctx);

  /*!
   * \brief Move src_ptr to memory from allocator
   *
   * \param src_ptr Pointer to move.
   * \param allocator Allocator to use to allocate new memory for moved data.
   *
   * \return Pointer to new location of data.
   */
  void* move(void* src_ptr, Allocator allocator);

  /*!
   * \brief Deallocate any pointer allocated by an Umpire-managed resource.
   *
   * \param ptr Pointer to deallocate.
   */
  void deallocate(void* ptr);

  /*!
   * \brief Asynchronously prefetch memory ptr to device.
   *
   * \param ptr Pointer to prefech
   * \param device Device to prefetch data to
   * \param ctx Resource to use for asynchronous operation
   */
  camp::resources::EventProxy<camp::resources::Resource> prefetch(void* ptr, int device,
                                                                  camp::resources::Resource& ctx);

  /*!
   * \brief Get the size in bytes of the allocation for the given pointer.
   *
   * \param ptr Pointer to find size of.
   *
   * \return Size of allocation in bytes.
   */
  std::size_t getSize(void* ptr) const;

  std::size_t getInternalMemoryUsage() const;

  std::shared_ptr<op::MemoryOperation> getOperation(const std::string& operation_name, Allocator src_allocator,
                                                    Allocator dst_allocator);

  int getNumDevices() const;

  ~ResourceManager();
  ResourceManager(const ResourceManager&) = delete;
  ResourceManager& operator=(const ResourceManager&) = delete;

 private:
  ResourceManager();

  strategy::AllocationStrategy* findAllocatorForPointer(void* ptr);
  strategy::AllocationStrategy* findAllocatorForId(int id);
  strategy::AllocationStrategy* getAllocationStrategy(const std::string& name);

  /*!
   * \brief Find the allocation record for the given pointer.
   *
   * With UMPIRE_ENABLE_INTROSPECTION_HEADER, the record is reconstructed
   * from the allocation header into thread-local storage, so the returned
   * pointer is a transient view that is only valid until a small number of
   * subsequent lookups on the same thread.
   */
  util::AllocationRecord* findAllocationRecordInternal(void* ptr) const;

  bool isBuiltinAllocator(strategy::AllocationStrategy* strategy);

  /*!
   * \brief Check if strict destruction mode is enabled via environment variable.
   *
   * Checks the UMPIRE_STRICT_DESTRUCTION environment variable. If set to any
   * value, strict mode is enabled (errors thrown). If unset, non-strict mode
   * is used (warnings logged). The check is cached on first call.
   *
   * \return true if UMPIRE_STRICT_DESTRUCTION is set, false otherwise
   */
  bool isStrictDestructionMode() const noexcept;

  int getNextId() noexcept;

  std::string getAllocatorInformation() const noexcept;

  strategy::AllocationStrategy* getZeroByteAllocator();

  void* reallocate_impl(void* current_ptr, std::size_t new_size, Allocator allocator);

  void* reallocate_impl(void* current_ptr, std::size_t new_size, Allocator allocator, camp::resources::Resource& ctx);

  util::AllocationMap m_allocations;

  std::list<std::unique_ptr<strategy::AllocationStrategy>> m_allocators;
  std::vector<std::string> m_shared_allocator_names;

  std::unordered_map<int, strategy::AllocationStrategy*> m_allocators_by_id;
  std::unordered_map<std::string, strategy::AllocationStrategy*> m_allocators_by_name;
  std::unordered_map<resource::MemoryResourceType, strategy::AllocationStrategy*, resource::MemoryResourceTypeHash>
      m_memory_resources;

  strategy::AllocationStrategy* m_default_allocator{nullptr};
  strategy::AllocationStrategy* m_null_allocator{nullptr};
  strategy::AllocationStrategy* m_zero_byte_pool{nullptr};

  int m_id;

  std::mutex m_mutex;

  // Methods that need access to m_allocations to print/filter records
  friend void print_allocator_records(Allocator, std::ostream&);
  friend std::vector<util::AllocationRecord> get_allocator_records(Allocator);
  friend strategy::ZeroByteHandler;
  friend strategy::mixins::AllocateNull;
};

} // end namespace umpire

#include "umpire/ResourceManager.inl"

#endif // UMPIRE_ResourceManager_HPP
