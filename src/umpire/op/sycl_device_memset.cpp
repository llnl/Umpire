//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-25, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "umpire/op/sycl.hpp"

namespace umpire {
namespace op {
namespace detail {

template <typename T>
void device_memset_sycl(T* ptr, T value, std::size_t count, sycl::queue& queue)
{
  if (!ptr || count == 0) {
    return;
  }

  // Launch parallel kernel
  auto event = queue.parallel_for(sycl::range<1>(count), [=](sycl::id<1> idx) {
    ptr[idx] = value;
  });

  // Wait for completion (synchronous operation), consistent with the
  // wait_and_throw()-based error handling used elsewhere in the new SYCL ops
  // (see sycl_error_check in sycl.hpp).
  sycl_error_check(event, "SYCL device_memset failed");
}

// Explicit template instantiations for common types
template void device_memset_sycl<char>(char*, char, std::size_t, sycl::queue&);
template void device_memset_sycl<unsigned char>(unsigned char*, unsigned char, std::size_t, sycl::queue&);
template void device_memset_sycl<short>(short*, short, std::size_t, sycl::queue&);
template void device_memset_sycl<unsigned short>(unsigned short*, unsigned short, std::size_t, sycl::queue&);
template void device_memset_sycl<int>(int*, int, std::size_t, sycl::queue&);
template void device_memset_sycl<unsigned int>(unsigned int*, unsigned int, std::size_t, sycl::queue&);
template void device_memset_sycl<long>(long*, long, std::size_t, sycl::queue&);
template void device_memset_sycl<unsigned long>(unsigned long*, unsigned long, std::size_t, sycl::queue&);
template void device_memset_sycl<long long>(long long*, long long, std::size_t, sycl::queue&);
template void device_memset_sycl<unsigned long long>(unsigned long long*, unsigned long long, std::size_t, sycl::queue&);
template void device_memset_sycl<float>(float*, float, std::size_t, sycl::queue&);
template void device_memset_sycl<double>(double*, double, std::size_t, sycl::queue&);

} // namespace detail
} // namespace op
} // namespace umpire
