#include "umpire/op/cuda.hpp"

namespace umpire {
namespace op {

// CUDA implementation helpers
namespace detail {

/*!
 * \brief Device kernel to set array elements to a value.
 *
 * This kernel sets each element of the array to the specified value.
 * Unlike standard memset which sets bytes, this sets typed elements.
 *
 * \tparam T The type of elements in the array
 * \param data Pointer to the array
 * \param value The value to set each element to
 * \param count The number of elements to set
 */
template <typename T>
__global__ void umpire_device_memset_kernel(T* data, T value, std::size_t count)
{
  const std::size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
  const std::size_t stride = blockDim.x * gridDim.x;

  for (std::size_t i = idx; i < count; i += stride) {
    data[i] = value;
  }
}

template <typename T>
void device_memset(T* ptr, T value, std::size_t count)
{
  if (!ptr || count == 0) {
    return;
  }

  constexpr int block_size = 256;
  std::size_t grid_size = (count + block_size - 1) / block_size;

  const std::size_t max_blocks = 65535;
  if (grid_size > max_blocks) {
    grid_size = max_blocks;
  }

  umpire_device_memset_kernel<T><<<grid_size, block_size>>>(ptr, value, count);

  cudaError_t err = cudaGetLastError();
  if (err != cudaSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("device_memset kernel launch failed: {}", cudaGetErrorString(err)));
  }

  // Synchronize to ensure kernel completion for synchronous operation
  cudaError_t sync_err = cudaDeviceSynchronize();
  if (sync_err != cudaSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("device_memset synchronization failed: {}", cudaGetErrorString(sync_err)));
  }
}

// Explicit template instantiations for common types
template __global__ void umpire_device_memset_kernel<char>(char*, char, std::size_t);
template __global__ void umpire_device_memset_kernel<unsigned char>(unsigned char*, unsigned char, std::size_t);
template __global__ void umpire_device_memset_kernel<short>(short*, short, std::size_t);
template __global__ void umpire_device_memset_kernel<unsigned short>(unsigned short*, unsigned short, std::size_t);
template __global__ void umpire_device_memset_kernel<int>(int*, int, std::size_t);
template __global__ void umpire_device_memset_kernel<unsigned int>(unsigned int*, unsigned int, std::size_t);
template __global__ void umpire_device_memset_kernel<long>(long*, long, std::size_t);
template __global__ void umpire_device_memset_kernel<unsigned long>(unsigned long*, unsigned long, std::size_t);
template __global__ void umpire_device_memset_kernel<long long>(long long*, long long, std::size_t);
template __global__ void umpire_device_memset_kernel<unsigned long long>(unsigned long long*, unsigned long long, std::size_t);
template __global__ void umpire_device_memset_kernel<float>(float*, float, std::size_t);
template __global__ void umpire_device_memset_kernel<double>(double*, double, std::size_t);

// Explicit template instantiations for device_memset function
template void device_memset<char>(char*, char, std::size_t);
template void device_memset<unsigned char>(unsigned char*, unsigned char, std::size_t);
template void device_memset<short>(short*, short, std::size_t);
template void device_memset<unsigned short>(unsigned short*, unsigned short, std::size_t);
template void device_memset<int>(int*, int, std::size_t);
template void device_memset<unsigned int>(unsigned int*, unsigned int, std::size_t);
template void device_memset<long>(long*, long, std::size_t);
template void device_memset<unsigned long>(unsigned long*, unsigned long, std::size_t);
template void device_memset<long long>(long long*, long long, std::size_t);
template void device_memset<unsigned long long>(unsigned long long*, unsigned long long, std::size_t);
template void device_memset<float>(float*, float, std::size_t);
template void device_memset<double>(double*, double, std::size_t);

} //end namespace detail

} //end namespace op

} //end namespace umpire
