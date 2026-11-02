#include "umpire/op/hip.hpp"

namespace umpire {
namespace op {

// HIP implementation helpers
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

/*!
 * \brief Launch the device_memset kernel on the given stream and device.
 *
 * Sets the current device to \p device for the launch (and optional stream
 * synchronization), then restores the previously current device before
 * reporting any error.
 */
template <typename T>
void launch_device_memset(T* ptr, T value, std::size_t count, hipStream_t stream, int device, bool synchronize)
{
  if (!ptr || count == 0) {
    return;
  }

  int previous_device;
  hipError_t get_dev_err = hipGetDevice(&previous_device);
  if (get_dev_err != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipGetDevice failed with error: {}", hipGetErrorString(get_dev_err)));
  }

  hipError_t set_dev_err = hipSetDevice(device);
  if (set_dev_err != hipSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("hipSetDevice(device={}) failed with error: {}", device, hipGetErrorString(set_dev_err)));
  }

  constexpr int block_size = 256;
  std::size_t grid_size = (count + block_size - 1) / block_size;

  const std::size_t max_blocks = 65535;
  if (grid_size > max_blocks) {
    grid_size = max_blocks;
  }

  hipLaunchKernelGGL(umpire_device_memset_kernel<T>, dim3(grid_size), dim3(block_size), 0, stream,
                     ptr, value, count);

  hipError_t err = hipGetLastError();

  // Synchronize to ensure kernel completion for synchronous operation
  hipError_t sync_err = hipSuccess;
  if (err == hipSuccess && synchronize) {
    sync_err = hipStreamSynchronize(stream);
  }

  hipError_t restore_err = hipSetDevice(previous_device);

  if (err != hipSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("device_memset kernel launch failed: {}", hipGetErrorString(err)));
  }

  if (sync_err != hipSuccess) {
    UMPIRE_ERROR(runtime_error,
                 fmt::format("device_memset synchronization failed: {}", hipGetErrorString(sync_err)));
  }

  if (restore_err != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipSetDevice(device={}) failed with error: {}", previous_device,
                                            hipGetErrorString(restore_err)));
  }
}

template <typename T>
void device_memset(T* ptr, T value, std::size_t count)
{
  if (!ptr || count == 0) {
    return;
  }

  // Run on the device that owns ptr, not whatever device is current.
  hipPointerAttribute_t attributes;
  hipError_t attr_err = hipPointerGetAttributes(&attributes, ptr);
  if (attr_err != hipSuccess) {
    UMPIRE_ERROR(runtime_error, fmt::format("hipPointerGetAttributes(ptr={}) failed with error: {}", fmt::ptr(ptr),
                                            hipGetErrorString(attr_err)));
  }

  int device = attributes.device;
  if (device < 0) {
    // Not associated with a device (e.g. system-allocated memory): use the current device.
    hipError_t get_dev_err = hipGetDevice(&device);
    if (get_dev_err != hipSuccess) {
      UMPIRE_ERROR(runtime_error, fmt::format("hipGetDevice failed with error: {}", hipGetErrorString(get_dev_err)));
    }
  }

  launch_device_memset(ptr, value, count, nullptr, device, true);
}

template <typename T>
void device_memset(T* ptr, T value, std::size_t count, hipStream_t stream, int device)
{
  launch_device_memset(ptr, value, count, stream, device, false);
}

// Explicit template instantiations for common types
template __global__ void umpire_device_memset_kernel<char>(char*, char, std::size_t);
template __global__ void umpire_device_memset_kernel<signed char>(signed char*, signed char, std::size_t);
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
template __global__ void umpire_device_memset_kernel<bool>(bool*, bool, std::size_t);

// Explicit template instantiations for device_memset function
template void device_memset<char>(char*, char, std::size_t);
template void device_memset<signed char>(signed char*, signed char, std::size_t);
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
template void device_memset<bool>(bool*, bool, std::size_t);

// Explicit template instantiations for asynchronous device_memset function
template void device_memset<char>(char*, char, std::size_t, hipStream_t, int);
template void device_memset<signed char>(signed char*, signed char, std::size_t, hipStream_t, int);
template void device_memset<unsigned char>(unsigned char*, unsigned char, std::size_t, hipStream_t, int);
template void device_memset<short>(short*, short, std::size_t, hipStream_t, int);
template void device_memset<unsigned short>(unsigned short*, unsigned short, std::size_t, hipStream_t, int);
template void device_memset<int>(int*, int, std::size_t, hipStream_t, int);
template void device_memset<unsigned int>(unsigned int*, unsigned int, std::size_t, hipStream_t, int);
template void device_memset<long>(long*, long, std::size_t, hipStream_t, int);
template void device_memset<unsigned long>(unsigned long*, unsigned long, std::size_t, hipStream_t, int);
template void device_memset<long long>(long long*, long long, std::size_t, hipStream_t, int);
template void device_memset<unsigned long long>(unsigned long long*, unsigned long long, std::size_t, hipStream_t, int);
template void device_memset<float>(float*, float, std::size_t, hipStream_t, int);
template void device_memset<double>(double*, double, std::size_t, hipStream_t, int);
template void device_memset<bool>(bool*, bool, std::size_t, hipStream_t, int);

} //end namespace detail

} //end namespace op

} //end namespace umpire
