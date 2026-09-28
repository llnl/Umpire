//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <iostream>

#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/op.hpp"

constexpr int BLOCK_SIZE = 256;
constexpr int NUM_THREADS = 4096;
constexpr int MULTIPLE = 1024;

__global__ void touch_data(double* data, int len)
{
  int id = blockIdx.x * blockDim.x + threadIdx.x;

  if (id < len) {
    data[id] = id * MULTIPLE;
  }
}

int main(int, char**)
{
  auto& rm = umpire::ResourceManager::getInstance();

  std::cout << "Available allocators: ";
  for (auto s : rm.getResourceNames()) {
    std::cout << s << "  ";
  }
  std::cout << std::endl;

  auto pool0 = rm.makeAllocator<umpire::strategy::QuickPool>("pool0", rm.getAllocator("DEVICE::0"));

  auto pool1 = rm.makeAllocator<umpire::strategy::QuickPool>("pool1", rm.getAllocator("DEVICE::1"));

  double* a = static_cast<double*>(pool0.allocate(NUM_THREADS * sizeof(double)));
  double* b = static_cast<double*>(pool1.allocate(NUM_THREADS * sizeof(double)));

  int NUM_BLOCKS = NUM_THREADS / BLOCK_SIZE;

#if defined(UMPIRE_ENABLE_CUDA)
  cudaError_t err = cudaSetDevice(0);
  if (err != cudaSuccess) {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Error when trying to set CUDA Device: {}", cudaGetErrorString(err)));
  }
  touch_data<<<NUM_BLOCKS, BLOCK_SIZE>>>(a, NUM_THREADS);
  err = cudaDeviceSynchronize();
  if (err != cudaSuccess) {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Error when trying to sync CUDA Device: {}", cudaGetErrorString(err)));
  }
#endif
#if defined(UMPIRE_ENABLE_HIP)
  hipError_t err = hipSetDevice(0);
  if (err != hipSuccess) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("Error when trying to set HIP Device: {}", hipGetErrorString(err)));
  }
  hipLaunchKernelGGL(touch_data, dim3(NUM_BLOCKS), dim3(BLOCK_SIZE), 0, 0, a, NUM_THREADS);
  err = hipDeviceSynchronize();
  if (err != hipSuccess) {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Error when trying to sync HIP Device: {}", hipGetErrorString(err)));
  }
#endif

  umpire::copy(a, b, NUM_THREADS);
  b = static_cast<double*>(rm.move(b, rm.getAllocator("HOST")));

  UMPIRE_ASSERT(b[BLOCK_SIZE] == (BLOCK_SIZE * MULTIPLE) && "Error: incorrect value!");
  std::cout << "Kernel succeeded! Expected result returned - " << b[BLOCK_SIZE] << std::endl;

  rm.deallocate(a);
  rm.deallocate(b);
}
