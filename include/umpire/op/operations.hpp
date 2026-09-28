#pragma once

#include <cstdlib>
#include <functional>
#include <optional>
#include <stdexcept>

#include "camp/resource.hpp"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/config.hpp"

namespace umpire {
namespace op {

struct operation {
  static constexpr int arity = -1;
  static constexpr const char* name = "UNKNOWN";
};

template <typename Src, typename Dst>
struct copy : public operation {
  static constexpr int arity = 2;
  static constexpr const char* name = "COPY";
};

template <typename Src>
struct memset : public operation {
  static constexpr int arity = 1;
  static constexpr const char* name = "MEMSET";
};

template <typename Src>
struct device_memset : public operation {
  static constexpr int arity = 1;
  static constexpr const char* name = "DEVICE_MEMSET";
};

template <typename Platform>
struct reallocate : public operation {
  static constexpr int arity = 1;
  static constexpr const char* name = "REALLOCATE";

  template <typename T>
  static T* exec(T** ptr, std::size_t new_size);

  template <typename T>
  static camp::resources::EventProxy<camp::resources::Resource> exec(T** ptr_ptr, std::size_t new_size,
                                                                     camp::resources::Resource& ctx);

  static void* exec(void** ptr_ptr, std::size_t new_size);

  static camp::resources::EventProxy<camp::resources::Resource> exec(void** ptr_ptr, std::size_t new_size,
                                                                     camp::resources::Resource& ctx);

private:
  // Private implementation helpers containing common reallocate logic
  template <typename PtrType>
  static PtrType* reallocate_impl_sync(PtrType** ptr_ptr, std::size_t new_size);

  template <typename PtrType>
  static camp::resources::EventProxy<camp::resources::Resource> reallocate_impl_async(
      PtrType** ptr_ptr, std::size_t new_size, camp::resources::Resource& ctx);
};

#define DEFINE_ADVICE_OP(op_name, name_str)                                                        \
  template <typename Src>                                                                          \
  struct op_name : public operation {                                                              \
    static constexpr int arity = 1;                                                                \
    static constexpr const char* name = #op_name;                                                  \
                                                                                                   \
    static void exec(void*, int, std::size_t)                                                      \
    {                                                                                              \
      throw std::runtime_error("Memory advice not supported for this platform");                   \
    }                                                                                              \
                                                                                                   \
    static camp::resources::EventProxy<camp::resources::Resource> exec(void*, int, std::size_t,    \
                                                                       camp::resources::Resource&) \
    {                                                                                              \
      throw std::runtime_error("Memory advice not supported for this platform");                   \
    }                                                                                              \
  };

DEFINE_ADVICE_OP(set_accessed_by, "SET_ACCESSED_BY")
DEFINE_ADVICE_OP(unset_accessed_by, "UNSET_ACCESSED_BY")
DEFINE_ADVICE_OP(set_preferred_location, "SET_PREFERRED_LOCATION")
DEFINE_ADVICE_OP(unset_preferred_location, "UNSET_PREFERRED_LOCATION")
DEFINE_ADVICE_OP(set_read_mostly, "SET_READ_MOSTLY")
DEFINE_ADVICE_OP(unset_read_mostly, "UNSET_READ_MOSTLY")
DEFINE_ADVICE_OP(set_coarse_grain, "SET_COARSE_GRAIN")
DEFINE_ADVICE_OP(unset_coarse_grain, "UNSET_COARSE_GRAIN")
DEFINE_ADVICE_OP(prefetch, "PREFETCH")

#undef DEFINE_ADVICE_OP

} // namespace op
} // namespace umpire
