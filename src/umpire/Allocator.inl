//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_Allocator_INL
#define UMPIRE_Allocator_INL

#include "umpire/Allocator.hpp"
#include "umpire/config.hpp"
#include "umpire/event/event.hpp"
#include "umpire/event/recorder_factory.hpp"
#include "umpire/strategy/ThreadSafeAllocator.hpp"
#include "umpire/util/AllocationHeader.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"

namespace umpire {

inline void* Allocator::do_allocate(std::size_t bytes)
{
  void* ret = nullptr;

  UMPIRE_ASSERT(UMPIRE_VERSION_OK());

  UMPIRE_LOG(Debug, "(" << bytes << ")");

#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  if (m_tracking) {
    // Zero-byte allocations also carry a header so that the owning strategy
    // can always be recovered from the pointer
    try {
      void* base_ptr{m_allocator->allocate(bytes + util::allocation_header_size)};
      ret = util::write_allocation_header(base_ptr, bytes, m_allocator);
    } catch (umpire::out_of_memory_error& e) {
      e.set_allocator_id(this->getId());
      e.set_requested_size(bytes);
      throw;
    }
  } else if (0 == bytes) {
    ret = allocateNull();
  } else {
    try {
      ret = m_allocator->allocate(bytes);
    } catch (umpire::out_of_memory_error& e) {
      e.set_allocator_id(this->getId());
      e.set_requested_size(bytes);
      throw;
    }
  }
#else
  if (0 == bytes) {
    ret = allocateNull();
  } else {
    try {
      ret = m_allocator->allocate(bytes);
    } catch (umpire::out_of_memory_error& e) {
      e.set_allocator_id(this->getId());
      e.set_requested_size(bytes);
      throw;
    }
  }
#endif

  if (m_tracking) {
    registerAllocation(ret, bytes, m_allocator);
  }

  umpire::event::record<umpire::event::allocate>(
      [&](auto& event) { event.size(bytes).ref((void*)m_allocator).ptr(ret); });

  return ret;
}

inline void* Allocator::thread_safe_allocate(std::size_t bytes)
{
  std::lock_guard<std::mutex> lock(*m_thread_safe_mutex);
  return do_allocate(bytes);
}

inline void* Allocator::thread_safe_resource_allocate(std::size_t bytes, camp::resources::Resource const& r)
{
  std::lock_guard<std::mutex> lock(*m_thread_safe_mutex);
  return do_resource_allocate(bytes, r);
}

inline void* Allocator::thread_safe_named_allocate(const std::string& name, std::size_t bytes)
{
  std::lock_guard<std::mutex> lock(*m_thread_safe_mutex);
  return do_named_allocate(name, bytes);
}

inline void Allocator::thread_safe_deallocate(void* ptr)
{
  std::lock_guard<std::mutex> lock(*m_thread_safe_mutex);
  return do_deallocate(ptr);
}

inline void Allocator::thread_safe_resource_deallocate(void* ptr, camp::resources::Resource const& r)
{
  std::lock_guard<std::mutex> lock(*m_thread_safe_mutex);
  return do_resource_deallocate(ptr, r);
}

inline void* Allocator::do_named_allocate(const std::string& name, std::size_t bytes)
{
  void* ret = nullptr;

  UMPIRE_ASSERT(UMPIRE_VERSION_OK());

  UMPIRE_LOG(Debug, "(" << bytes << ")");

#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  if (m_tracking) {
    // Zero-byte allocations also carry a header so that the owning strategy
    // can always be recovered from the pointer
    void* base_ptr{m_allocator->allocate_named(name, bytes + util::allocation_header_size)};
    ret = util::write_allocation_header(base_ptr, bytes, m_allocator);
  } else if (0 == bytes) {
    ret = allocateNull();
  } else {
    ret = m_allocator->allocate_named(name, bytes);
  }
#else
  if (0 == bytes) {
    ret = allocateNull();
  } else {
    ret = m_allocator->allocate_named(name, bytes);
  }
#endif

  if (m_tracking) {
    registerAllocation(ret, bytes, m_allocator, name);
  }

  umpire::event::record<umpire::event::named_allocate>(
      [&](auto& event) { event.name(name).size(bytes).ref((void*)m_allocator).ptr(ret); });
  return ret;
}

inline void* Allocator::do_resource_allocate(std::size_t bytes, camp::resources::Resource const& r)
{
  void* ret = nullptr;

  UMPIRE_ASSERT(UMPIRE_VERSION_OK());

  UMPIRE_LOG(Debug, "(" << bytes << ")");

#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
  if (m_tracking) {
    // Zero-byte allocations also carry a header so that the owning strategy
    // can always be recovered from the pointer
    void* base_ptr{m_allocator->allocate_resource(bytes + util::allocation_header_size, r)};
    ret = util::write_allocation_header(base_ptr, bytes, m_allocator);
  } else if (0 == bytes) {
    ret = allocateNull();
  } else {
    ret = m_allocator->allocate_resource(bytes, r);
  }
#else
  if (0 == bytes) {
    ret = allocateNull();
  } else {
    ret = m_allocator->allocate_resource(bytes, r);
  }
#endif

  if (m_tracking) {
    registerAllocation(ret, bytes, m_allocator);
  }

  umpire::event::record<umpire::event::allocate_resource>(
      [&](auto& event) { event.size(bytes).ref((void*)m_allocator).ptr(ret).res(camp::resources::to_string(r)); });
  return ret;
}

inline void Allocator::do_deallocate(void* ptr)
{
  umpire::event::record<umpire::event::deallocate>([&](auto& event) { event.ref((void*)m_allocator).ptr(ptr); });

  UMPIRE_LOG(Debug, "(" << ptr << ")");

  if (!ptr) {
    UMPIRE_LOG(Info, "Deallocating a null pointer (This behavior is intentionally allowed and ignored)");
    return;
  } else {
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
    if (m_tracking) {
      // Zero-byte allocations carry no header, so they must be checked first
      if (deallocateNull(ptr)) {
        deregisterNullAllocation(m_allocator);
      } else {
        auto record = deregisterAllocation(ptr, m_allocator);
        m_allocator->deallocate(util::get_base_pointer(ptr), record.size + util::allocation_header_size);
      }
    } else {
      if (!deallocateNull(ptr)) {
        m_allocator->deallocate(ptr);
      }
    }
#else
    if (m_tracking) {
      auto record = deregisterAllocation(ptr, m_allocator);
      if (!deallocateNull(ptr)) {
        m_allocator->deallocate(ptr, record.size);
      }
    } else {
      if (!deallocateNull(ptr)) {
        m_allocator->deallocate(ptr);
      }
    }
#endif
  }
}

inline void Allocator::do_resource_deallocate(void* ptr, camp::resources::Resource const& r)
{
  umpire::event::record<umpire::event::deallocate_resource>(
      [&](auto& event) { event.ref((void*)m_allocator).ptr(ptr).res(camp::resources::to_string(r)); });

  UMPIRE_LOG(Debug, "(" << ptr << ")");

  if (!ptr) {
    UMPIRE_LOG(Info, "Deallocating a null pointer (This behavior is intentionally allowed and ignored)");
    return;
  } else {
#if defined(UMPIRE_ENABLE_INTROSPECTION_HEADER)
    if (m_tracking) {
      // Zero-byte allocations carry no header, so they must be checked first
      if (deallocateNull(ptr)) {
        deregisterNullAllocation(m_allocator);
      } else {
        auto record = deregisterAllocation(ptr, m_allocator);
        m_allocator->deallocate_resource(util::get_base_pointer(ptr), r, record.size + util::allocation_header_size);
      }
    } else {
      if (!deallocateNull(ptr)) {
        m_allocator->deallocate_resource(ptr, r);
      }
    }
#else
    if (m_tracking) {
      auto record = deregisterAllocation(ptr, m_allocator);
      if (!deallocateNull(ptr)) {
        m_allocator->deallocate_resource(ptr, r, record.size);
      }
    } else {
      if (!deallocateNull(ptr)) {
        m_allocator->deallocate_resource(ptr, r);
      }
    }
#endif
  }
}

inline void* Allocator::allocate(std::size_t bytes)
{
  return m_thread_safe ? thread_safe_allocate(bytes) : do_allocate(bytes);
}

inline void* Allocator::allocate(std::size_t bytes, camp::resources::Resource const& r)
{
  return m_thread_safe ? thread_safe_resource_allocate(bytes, r) : do_resource_allocate(bytes, r);
}

inline void* Allocator::allocate(const std::string& name, std::size_t bytes)
{
  return m_thread_safe ? thread_safe_named_allocate(name, bytes) : do_named_allocate(name, bytes);
}

inline void Allocator::deallocate(void* ptr)
{
  m_thread_safe ? thread_safe_deallocate(ptr) : do_deallocate(ptr);
}

inline void Allocator::deallocate(void* ptr, camp::resources::Resource const& r)
{
  m_thread_safe ? thread_safe_resource_deallocate(ptr, r) : do_resource_deallocate(ptr, r);
}

} // end of namespace umpire

#endif // UMPIRE_Allocator_INL
