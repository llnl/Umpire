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
#include "umpire/event/operation_recording.hpp"
#include "umpire/strategy/ThreadSafeAllocator.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"

namespace umpire {

inline void* Allocator::do_allocate(std::size_t bytes)
{
  UMPIRE_ASSERT(UMPIRE_VERSION_OK());

  UMPIRE_LOG(Debug, "(" << bytes << ")");

  return umpire::event::record_allocate(m_allocator, bytes, [&]() -> void* {
    void* ret = nullptr;

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

    if (m_tracking) {
      registerAllocation(ret, bytes, m_allocator);
    }

    return ret;
  });
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
  UMPIRE_ASSERT(UMPIRE_VERSION_OK());

  UMPIRE_LOG(Debug, "(" << bytes << ")");

  return umpire::event::record_named_allocate(m_allocator, name, bytes, [&]() -> void* {
    void* ret = nullptr;

    if (0 == bytes) {
      ret = allocateNull();
    } else {
      ret = m_allocator->allocate_named(name, bytes);
    }

    if (m_tracking) {
      registerAllocation(ret, bytes, m_allocator, name);
    }

    return ret;
  });
}

inline void* Allocator::do_resource_allocate(std::size_t bytes, camp::resources::Resource const& r)
{
  UMPIRE_ASSERT(UMPIRE_VERSION_OK());

  UMPIRE_LOG(Debug, "(" << bytes << ")");

  return umpire::event::record_resource_allocate(m_allocator, bytes, r, [&]() -> void* {
    void* ret = nullptr;

    if (0 == bytes) {
      ret = allocateNull();
    } else {
      ret = m_allocator->allocate_resource(bytes, r);
    }

    if (m_tracking) {
      registerAllocation(ret, bytes, m_allocator);
    }

    return ret;
  });
}

inline void Allocator::do_deallocate(void* ptr)
{
  UMPIRE_LOG(Debug, "(" << ptr << ")");

  umpire::event::record_deallocate(m_allocator, ptr, [&]() {
    if (!ptr) {
      UMPIRE_LOG(Info, "Deallocating a null pointer (This behavior is intentionally allowed and ignored)");
      return;
    } else {
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
    }
  });
}

inline void Allocator::do_resource_deallocate(void* ptr, camp::resources::Resource const& r)
{
  UMPIRE_LOG(Debug, "(" << ptr << ")");

  umpire::event::record_resource_deallocate(m_allocator, ptr, r, [&]() {
    if (!ptr) {
      UMPIRE_LOG(Info, "Deallocating a null pointer (This behavior is intentionally allowed and ignored)");
      return;
    } else {
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
    }
  });
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
