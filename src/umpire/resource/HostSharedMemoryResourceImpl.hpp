//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef __HOST_SHARED_MEMORY_RESOURCE_HPP__
#define __HOST_SHARED_MEMORY_RESOURCE_HPP__

#include <fcntl.h> // For O_* constants
#include <pthread.h>
#include <string.h>    // strerror
#include <sys/mman.h>  // mmap
#include <sys/stat.h>  // For mode constants, fstat
#include <sys/types.h> // ftruncate, fstat
#include <unistd.h>    // ftruncate, fstat

#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>

#include "umpire/resource/HostSharedMemoryResource.hpp"
#include "umpire/resource/MemoryResource.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"

namespace umpire {
namespace resource {

class HostSharedMemoryResource::impl {
  struct SharedMemoryBlock {
    std::size_t next_block_off; // Offset == 0 is same as nullptr
    std::size_t name_offset;
    std::size_t memory_offset;
    std::size_t block_size; // Includes header+name+memory
    std::size_t reference_count;
  };

  struct SharedMemorySegmentHeader {
    uint32_t init_flag;
    uint32_t reserved;
    pthread_mutex_t mutex;
    std::size_t segment_size; // Full segment size, including this header
    std::size_t actual_size;  // Total current size of allocations+metadata
    std::size_t free_blocks_off;
    std::size_t used_blocks_off;
  };

 public:
  impl(const std::string& name, std::size_t size)
  {
    m_segment_name = (name[0] != '/') ? std::string{"/"} + name : name;
    const uint32_t Initializing{1};
    const uint32_t Initialized{2};

    bool created{false};
    bool completed{false};
    int err{0};

    UMPIRE_LOG(Debug, " ( "
                          << "name=\"" << name << "\""
                          << ", size=" << size << ")");

    //
    // SIMPLIFYING ASSUMPTION:
    //
    // This implementation depends upon something else cleaning up what
    // is placed in /dev/shm.  For LC, the epilogue scripts will remove
    // everything that is in /dev/shm so that new job allocations may be
    // assured of having a clean directory.
    //
    while (!completed) { // spin on opening shm
      if (open_shared_memory_segment(err, (O_RDWR | O_CREAT | O_EXCL))) {
        created = true;
        completed = true;
      } else if (err != EEXIST) {
        UMPIRE_ERROR(runtime_error,
                     fmt::format("Failed to create shared memory segment \"{}\": {}", m_segment_name, strerror(err)));
      } else {
        if (open_shared_memory_segment(err, O_RDWR)) {
          created = false;
          completed = true;
        } else if (err != ENOENT) {
          UMPIRE_ERROR(runtime_error,
                       fmt::format("Failed to open shared memory file \"{}\": {}", m_segment_name, strerror(err)));
        }
      }
      std::this_thread::yield();
    }

    if (created) {
      if (0 != ftruncate(m_segment_fd, size)) {
        err = errno;
        UMPIRE_ERROR(runtime_error, fmt::format("Failed to set size for shared memory segment \"{}\": {}",
                                                m_segment_name, strerror(err)));
      }

      map_shared_memory_segment();

      __atomic_store_n(&m_segment->init_flag, Initializing, __ATOMIC_SEQ_CST);

      pthread_mutexattr_t mattr;
      if ((err = pthread_mutexattr_init(&mattr)) != 0) {
        UMPIRE_ERROR(runtime_error,
                     fmt::format("Failed to initialize mutex attributes for shared memory segment \"{}\": {}",
                                 m_segment_name, strerror(err)));
      }

      if ((err = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED)) != 0) {
        UMPIRE_ERROR(runtime_error, fmt::format("Failed to set shared attributes for shared memory segment \"{}\": {}",
                                                m_segment_name, strerror(err)));
      }

      if ((err = pthread_mutex_init(&m_segment->mutex, &mattr)) != 0) {
        UMPIRE_ERROR(runtime_error, fmt::format("Failed to initialize mutex for shared memory segment \"{}\": {}",
                                                m_segment_name, strerror(err)));
      }

      m_segment->segment_size = size;

      pointer_to_offset(&m_segment[1], m_segment->free_blocks_off);
      m_segment->actual_size = m_segment->free_blocks_off;

      pointer_to_offset(nullptr, m_segment->used_blocks_off);
      SharedMemoryBlock* block_ptr;
      offset_to_pointer(m_segment->free_blocks_off, block_ptr);
      pointer_to_offset(nullptr, block_ptr->next_block_off);
      pointer_to_offset(nullptr, block_ptr->name_offset);
      block_ptr->block_size = size - sizeof(SharedMemorySegmentHeader);
      block_ptr->reference_count = 0;

      __atomic_store_n(&m_segment->init_flag, Initialized, __ATOMIC_SEQ_CST);
    } else {
      // Wait for the file size to change
      off_t filesize{0};
      while (filesize == 0) {
        struct stat st;

        if (fstat(m_segment_fd, &st) < 0) {
          err = errno;
          UMPIRE_ERROR(runtime_error,
                       fmt::format("Failed fstat for shared memory segment  {}: {}", m_segment_name, strerror(err)));
        }
        filesize = st.st_size;
        std::this_thread::yield();
      }

      map_shared_memory_segment();

      uint32_t value{__atomic_load_n(&m_segment->init_flag, __ATOMIC_SEQ_CST)};

      // Wait for the memory segment header to be initialized
      while (value != Initialized) {
        std::this_thread::yield();
        value = __atomic_load_n(&m_segment->init_flag, __ATOMIC_SEQ_CST);
      }
    }
  }

  ~impl()
  {
    int err{0};

    if ((err = munmap(m_segment, m_segment_size)) != 0) {
      err = errno;
      UMPIRE_LOG(Error, "Failed to unmap(m_segment=" << m_segment << ", m_segment_size=" << m_segment_size
                                                     << ") for segment " << m_segment_name << ": " << strerror(err));
    }

    if ((err = shm_unlink(m_segment_name.c_str())) != 0) {
      err = errno;
      UMPIRE_LOG(Error, "Failed to shm_unlink segment " << m_segment_name << ": " << strerror(err));
    }

    if ((err = close(m_segment_fd)) != 0) {
      err = errno;
      UMPIRE_LOG(Error, "Failed to close shared memory object " << m_segment_name << ": " << strerror(err));
    }
  }

  void* allocate_named(const std::string& name, std::size_t requested_size)
  {
    int err{0};
    void* ptr{nullptr};
    const std::size_t header_size{(sizeof(SharedMemoryBlock) + m_alignment) & ~(m_alignment - 1)};
    const std::size_t mem_size{(requested_size + m_alignment) & ~(m_alignment - 1)};
    const std::size_t name_size{(name.length() + 1 + m_alignment) & ~(m_alignment - 1)};
    const std::size_t adjusted_size{header_size + mem_size + name_size};

    UMPIRE_LOG(Debug, "(name=\"" << name << ", requested_size=" << requested_size << ")");

    if ((err = pthread_mutex_lock(&m_segment->mutex)) != 0) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Failed to lock mutex for shared memory segment {}: {}", m_segment_name, strerror(err)));
    }

    // First let's see if the allaction already exists
    SharedMemoryBlock* best{find_existing_allocation(name)};
    SharedMemoryBlock* prev{nullptr};

    if (best != nullptr) {
      best->reference_count++;
    } else { // New allocation
      findUsableBlock(best, prev, adjusted_size);

      if (best != nullptr) {
        // Split the free block
        splitBlock(best, prev, adjusted_size);

        m_segment->actual_size += best->block_size;

        // Push node to the list of used nodes
        best->next_block_off = m_segment->used_blocks_off;
        pointer_to_offset(best, m_segment->used_blocks_off);

        // Set up block header
        std::size_t block_offset;
        pointer_to_offset(best, block_offset);
        best->memory_offset = block_offset + header_size;
        best->name_offset = best->memory_offset + mem_size;
        best->reference_count = 1;

        char* name_ptr;
        offset_to_pointer(best->name_offset, name_ptr);
        name.copy(name_ptr, name.length(), 0);
        name_ptr[name.length()] = '\0';
      }
    }

    pthread_mutex_unlock(&m_segment->mutex);

    if (best == nullptr) {
      UMPIRE_ERROR(out_of_memory_error, fmt::format("shared memory allocation( bytes = {} ) failed", requested_size));
    } else {
      offset_to_pointer(best->memory_offset, ptr);
    }

    return ptr;
  }

  void deallocate(void* ptr)
  {
    UMPIRE_LOG(Debug, "(ptr=" << ptr << ")");

    const std::size_t header_size{(sizeof(SharedMemoryBlock) + m_alignment) & ~(m_alignment - 1)};
    std::size_t data_offset;
    pointer_to_offset(ptr, data_offset);

    std::size_t block_off{data_offset - header_size};
    SharedMemoryBlock* block_ptr;
    offset_to_pointer(block_off, block_ptr);

    int err{0};
    if ((err = pthread_mutex_lock(&m_segment->mutex)) != 0) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Failed to lock mutex for shared memory segment {}: {}", m_segment_name, strerror(err)));
    }

    block_ptr->reference_count--;

    if (block_ptr->reference_count == 0) {
      m_segment->actual_size -= block_ptr->block_size;

      block_ptr->memory_offset = 0;
      block_ptr->name_offset = 0;

      SharedMemoryBlock* curr;
      SharedMemoryBlock* prev{nullptr};

      offset_to_pointer(m_segment->used_blocks_off, curr);

      while (curr != nullptr && curr != block_ptr) {
        prev = curr;
        offset_to_pointer(curr->next_block_off, curr);
      }

      releaseBlock(block_ptr, prev);
    }

    pthread_mutex_unlock(&m_segment->mutex);
  }

  void* find_pointer_from_name(const std::string& name)
  {
    void* ptr{nullptr};
    int err{0};

    if ((err = pthread_mutex_lock(&m_segment->mutex)) != 0) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Failed to lock mutex for shared memory segment {}: {}", m_segment_name, strerror(err)));
    }

    // First let's see if the allaction already exists
    SharedMemoryBlock* block_ptr{find_existing_allocation(name)};

    if (block_ptr != nullptr) {
      offset_to_pointer(block_ptr->memory_offset, ptr);
    }

    pthread_mutex_unlock(&m_segment->mutex);
    return ptr;
  }

  void release()
  {
    int err{0};
    if ((err = pthread_mutex_lock(&m_segment->mutex)) != 0) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Failed to lock mutex for shared memory segment {}: {}", m_segment_name, strerror(err)));
    }

    long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
      page_size = 4096;
    }

    SharedMemoryBlock* block_ptr{nullptr};
    offset_to_pointer(m_segment->free_blocks_off, block_ptr);

    while (block_ptr != nullptr) {
      char* const block_begin = reinterpret_cast<char*>(block_ptr);
      char* const block_end = block_begin + block_ptr->block_size;

      constexpr std::size_t keep_bytes = sizeof(SharedMemoryBlock);

      std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(block_begin + keep_bytes);
      std::uintptr_t end = reinterpret_cast<std::uintptr_t>(block_end);

      const std::uintptr_t ps = static_cast<std::uintptr_t>(page_size);

      begin = (begin + ps - 1) & ~(ps - 1); // align up
      end = end & ~(ps - 1);               // align down

      if (end > begin) {
        void* advise_ptr = reinterpret_cast<void*>(begin);
        const std::size_t advise_len = static_cast<std::size_t>(end - begin);

        if (::madvise(advise_ptr, advise_len, MADV_DONTNEED) != 0) {
          int madvise_err = errno;
          UMPIRE_LOG(Debug, "madvise(MADV_DONTNEED) failed for shared memory segment "
                                << m_segment_name << ": " << strerror(madvise_err));
        }
      }

      offset_to_pointer(block_ptr->next_block_off, block_ptr);
    }

    if ((err = pthread_mutex_unlock(&m_segment->mutex)) != 0) {
      UMPIRE_ERROR(runtime_error, fmt::format("Failed to unlock mutex for shared memory segment {}: {}", m_segment_name,
                                              strerror(err)));
    }
  }

  std::size_t getActualSize() const noexcept
  {
    int err{0};
    std::size_t rval{0};

    if ((err = pthread_mutex_lock(&m_segment->mutex)) != 0) {
      UMPIRE_LOG(Error, "Failed to lock mutex. size not reliable for shared memory segment " << m_segment_name << ": "
                                                                                             << strerror(err));
    }

    rval = m_segment->actual_size;

    pthread_mutex_unlock(&m_segment->mutex);

    return rval;
  }

  Platform getPlatform() noexcept;

 private:
  std::string m_segment_name;
  int m_segment_fd{-1};
  std::size_t m_segment_size{0};
  SharedMemorySegmentHeader* m_segment{nullptr};
  std::size_t m_alignment{16};

  template <class OFF_T, class PTR_T>
  void offset_to_pointer(OFF_T offset, PTR_T& ptr)
  {
    if (offset == 0) {
      ptr = nullptr;
    } else {
      ptr = reinterpret_cast<PTR_T>(reinterpret_cast<char*>(m_segment) + offset);
    }
  }

  void pointer_to_offset(std::nullptr_t, std::size_t& offset)
  {
    offset = 0;
  }

  template <class PTR_T, class OFF_T>
  void pointer_to_offset(PTR_T ptr, OFF_T& offset)
  {
    char* base{reinterpret_cast<char*>(m_segment)};

    offset = ptr == nullptr ? 0 : static_cast<OFF_T>(reinterpret_cast<char*>(ptr) - base);
  }

  SharedMemoryBlock* find_existing_allocation(std::string name)
  {
    SharedMemoryBlock* block_ptr;
    offset_to_pointer(m_segment->used_blocks_off, block_ptr);

    while (block_ptr != nullptr) {
      char* allocation_name;
      offset_to_pointer(block_ptr->name_offset, allocation_name);
      if (allocation_name != nullptr && 0 == name.compare(allocation_name))
        break;
      offset_to_pointer(block_ptr->next_block_off, block_ptr);
    }

    return block_ptr;
  }

  void findUsableBlock(SharedMemoryBlock*& best, SharedMemoryBlock*& prev, std::size_t size)
  {
    best = prev = nullptr;

    SharedMemoryBlock* iter;
    offset_to_pointer(m_segment->free_blocks_off, iter);
    SharedMemoryBlock* iterPrev{nullptr};

    while (iter != nullptr) {
      if (iter->block_size >= size && (best == nullptr || iter->block_size < best->block_size)) {
        best = iter;
        prev = iterPrev;
        if (iter->block_size == size)
          break; // Exact match, won't find a better one, look no further
      }
      iterPrev = iter;
      offset_to_pointer(iter->next_block_off, iter);
    }
  }

  void releaseBlock(SharedMemoryBlock* curr, SharedMemoryBlock* prev)
  {
    if (prev)
      prev->next_block_off = curr->next_block_off;
    else
      m_segment->used_blocks_off = curr->next_block_off;

    // Find location to put this block in the freeBlocks list
    prev = nullptr;
    SharedMemoryBlock* iter;
    offset_to_pointer(m_segment->free_blocks_off, iter);

    while (iter != nullptr && iter < curr) {
      prev = iter;
      offset_to_pointer(iter->next_block_off, iter);
    }
    // Keep track of the successor
    std::size_t next_offset = prev ? prev->next_block_off : m_segment->free_blocks_off;
    SharedMemoryBlock* next;
    offset_to_pointer(next_offset, next);
    std::size_t prev_offset;
    pointer_to_offset(prev, prev_offset);
    std::size_t curr_offset;
    pointer_to_offset(curr, curr_offset);

    // Check if prev and curr can be merged
    if (prev && prev_offset + prev->block_size == curr_offset) {
      prev->block_size = prev->block_size + curr->block_size;
      curr = prev;
      pointer_to_offset(curr, curr_offset);
    } else if (prev) {
      prev->next_block_off = curr_offset;
    } else {
      m_segment->free_blocks_off = curr_offset;
    }

    // Check if curr and next can be merged
    if (next && (curr_offset + curr->block_size == next_offset)) {
      curr->block_size = curr->block_size + next->block_size;
      curr->next_block_off = next->next_block_off;
    } else {
      curr->next_block_off = next_offset;
    }
  }

  void splitBlock(SharedMemoryBlock*& curr, SharedMemoryBlock*& prev, const std::size_t size)
  {
    SharedMemoryBlock* next{nullptr};
    std::size_t remaining{curr->block_size - size};

    if (curr->block_size == size || remaining < sizeof(SharedMemoryBlock)) {
      // Keep it
      offset_to_pointer(curr->next_block_off, next);
    } else {
      // Split the block
      std::size_t curr_block_off;
      pointer_to_offset(curr, curr_block_off);

      SharedMemoryBlock* newBlock;
      offset_to_pointer(curr_block_off + size, newBlock);

      newBlock->memory_offset = 0;
      newBlock->name_offset = 0;
      newBlock->next_block_off = curr->next_block_off;
      newBlock->reference_count = 0;
      newBlock->block_size = remaining;

      next = newBlock;
      curr->block_size = size;
    }

    if (prev != nullptr) {
      std::size_t offset;
      pointer_to_offset(next, offset);
      prev->next_block_off = offset;
    } else {
      std::size_t offset;
      pointer_to_offset(next, offset);
      m_segment->free_blocks_off = offset;
    }
  }

  bool open_shared_memory_segment(int& err, int oflag)
  {
    constexpr int omode{S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH};

    m_segment_fd = shm_open(m_segment_name.c_str(), oflag, omode);
    err = errno;

    bool rval{m_segment_fd >= 0};

    if (rval && (oflag & O_CREAT)) {
      ::fchmod(m_segment_fd, omode);
    }

    return rval;
  }

  void map_shared_memory_segment()
  {
    struct ::stat buf;
    if (0 != fstat(m_segment_fd, &buf)) {
      int err = errno;
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Failed to obtain size of shared object \"{}\": {}", m_segment_name, strerror(err)));
    }

    auto size = buf.st_size;

    const int prot{PROT_WRITE | PROT_READ};
    const int flags{MAP_SHARED};

    void* base = mmap(nullptr, static_cast<std::size_t>(size), prot, flags, m_segment_fd, 0);

    if (base == MAP_FAILED) {
      int err = errno;
      UMPIRE_ERROR(runtime_error, fmt::format("Failed to map shared object \"{}\": {}", m_segment_name, strerror(err)));
    }

    m_segment = static_cast<SharedMemorySegmentHeader*>(base);
    m_segment_size = size;
  }
};

} // end of namespace resource
} // end of namespace umpire
#endif // __HOST_SHARED_MEMORY_RESOURCE_HPP__
