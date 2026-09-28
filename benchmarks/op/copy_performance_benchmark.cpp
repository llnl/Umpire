//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-25, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include <cstring>

#include "benchmark/benchmark.h"

#include "umpire/ResourceManager.hpp"
#include "umpire/Allocator.hpp"
#include "umpire/op.hpp"

constexpr int MIN_SIZE = 64;      // 64 bytes
constexpr int MAX_SIZE = 1048576; // 1 MB
constexpr int MULTIPLIER = 2;

//==============================================================================
// Benchmark 1: Original ResourceManager copy (legacy approach)
//==============================================================================

static void BM_ResourceManager_Copy(benchmark::State& state, const std::string& src_name, const std::string& dst_name) {
  auto& rm = umpire::ResourceManager::getInstance();

  auto src_allocator = rm.getAllocator(src_name);
  auto dst_allocator = rm.getAllocator(dst_name);

  const std::size_t size = state.range(0);

  void* src_ptr = src_allocator.allocate(size);
  void* dst_ptr = dst_allocator.allocate(size);

  // Initialize source data
  std::memset(src_ptr, 0xAA, size);

  for (auto _ : state) {
    // Original ResourceManager approach
    rm.copy(src_ptr, dst_ptr, size);
    benchmark::DoNotOptimize(dst_ptr);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(size));

  src_allocator.deallocate(src_ptr);
  dst_allocator.deallocate(dst_ptr);
}

//==============================================================================
// Benchmark 2: Runtime-dispatch copy (new operation system v2)
//==============================================================================

static void BM_RuntimeDispatch_Copy(benchmark::State& state, const std::string& src_name, const std::string& dst_name) {
  auto& rm = umpire::ResourceManager::getInstance();

  auto src_allocator = rm.getAllocator(src_name);
  auto dst_allocator = rm.getAllocator(dst_name);

  const std::size_t size = state.range(0);

  void* src_ptr = src_allocator.allocate(size);
  void* dst_ptr = dst_allocator.allocate(size);

  // Initialize source data
  std::memset(src_ptr, 0xBB, size);

  for (auto _ : state) {
    // Runtime dispatch - auto-detects platform from pointers
    umpire::copy(src_ptr, dst_ptr, size);
    benchmark::DoNotOptimize(dst_ptr);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(size));

  src_allocator.deallocate(src_ptr);
  dst_allocator.deallocate(dst_ptr);
}

//==============================================================================
// Benchmark 3: Compile-time dispatch copy (zero-overhead direct calls)
//==============================================================================

// Host to Host
static void BM_CompileTimeDispatch_Copy_Host_Host(benchmark::State& state) {
  auto& rm = umpire::ResourceManager::getInstance();

  auto src_allocator = rm.getAllocator("HOST");
  auto dst_allocator = rm.getAllocator("HOST");

  const std::size_t size = state.range(0);

  void* src_ptr = src_allocator.allocate(size);
  void* dst_ptr = dst_allocator.allocate(size);

  // Initialize source data
  std::memset(src_ptr, 0xCC, size);

  for (auto _ : state) {
    // Compile-time dispatch - explicit platform specification
    umpire::copy<umpire::resource::host_platform, umpire::resource::host_platform>(
        static_cast<unsigned char*>(src_ptr),
        static_cast<unsigned char*>(dst_ptr),
        size);
    benchmark::DoNotOptimize(dst_ptr);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(size));

  src_allocator.deallocate(src_ptr);
  dst_allocator.deallocate(dst_ptr);
}

#if defined(UMPIRE_ENABLE_CUDA)
// Host to CUDA Device
static void BM_CompileTimeDispatch_Copy_Host_Cuda(benchmark::State& state) {
  auto& rm = umpire::ResourceManager::getInstance();

  auto src_allocator = rm.getAllocator("HOST");
  auto dst_allocator = rm.getAllocator("DEVICE");

  const std::size_t size = state.range(0);

  void* src_ptr = src_allocator.allocate(size);
  void* dst_ptr = dst_allocator.allocate(size);

  // Initialize source data
  std::memset(src_ptr, 0xDD, size);

  for (auto _ : state) {
    // Compile-time dispatch - explicit platform specification
    umpire::copy<umpire::resource::host_platform, umpire::resource::cuda_platform>(
        static_cast<unsigned char*>(src_ptr),
        static_cast<unsigned char*>(dst_ptr),
        size);
    benchmark::DoNotOptimize(dst_ptr);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(size));

  src_allocator.deallocate(src_ptr);
  dst_allocator.deallocate(dst_ptr);
}

// CUDA Device to Host
static void BM_CompileTimeDispatch_Copy_Cuda_Host(benchmark::State& state) {
  auto& rm = umpire::ResourceManager::getInstance();

  auto src_allocator = rm.getAllocator("DEVICE");
  auto dst_allocator = rm.getAllocator("HOST");

  const std::size_t size = state.range(0);

  void* src_ptr = src_allocator.allocate(size);
  void* dst_ptr = dst_allocator.allocate(size);

  // Initialize source data on device
  unsigned char pattern = 0xEE;
  umpire::memset(src_ptr, pattern, size);

  for (auto _ : state) {
    // Compile-time dispatch - explicit platform specification
    umpire::copy<umpire::resource::cuda_platform, umpire::resource::host_platform>(
        static_cast<unsigned char*>(src_ptr),
        static_cast<unsigned char*>(dst_ptr),
        size);
    benchmark::DoNotOptimize(dst_ptr);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(size));

  src_allocator.deallocate(src_ptr);
  dst_allocator.deallocate(dst_ptr);
}

// CUDA Device to CUDA Device
static void BM_CompileTimeDispatch_Copy_Cuda_Cuda(benchmark::State& state) {
  auto& rm = umpire::ResourceManager::getInstance();

  auto src_allocator = rm.getAllocator("DEVICE");
  auto dst_allocator = rm.getAllocator("DEVICE");

  const std::size_t size = state.range(0);

  void* src_ptr = src_allocator.allocate(size);
  void* dst_ptr = dst_allocator.allocate(size);

  // Initialize source data on device
  unsigned char pattern = 0xFF;
  umpire::memset(src_ptr, pattern, size);

  for (auto _ : state) {
    // Compile-time dispatch - explicit platform specification
    umpire::copy<umpire::resource::cuda_platform, umpire::resource::cuda_platform>(
        static_cast<unsigned char*>(src_ptr),
        static_cast<unsigned char*>(dst_ptr),
        size);
    benchmark::DoNotOptimize(dst_ptr);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(size));

  src_allocator.deallocate(src_ptr);
  dst_allocator.deallocate(dst_ptr);
}
#endif // UMPIRE_ENABLE_CUDA

#if defined(UMPIRE_ENABLE_HIP)
// Host to HIP Device
static void BM_CompileTimeDispatch_Copy_Host_Hip(benchmark::State& state) {
  auto& rm = umpire::ResourceManager::getInstance();

  auto src_allocator = rm.getAllocator("HOST");
  auto dst_allocator = rm.getAllocator("DEVICE");

  const std::size_t size = state.range(0);

  void* src_ptr = src_allocator.allocate(size);
  void* dst_ptr = dst_allocator.allocate(size);

  // Initialize source data
  std::memset(src_ptr, 0x11, size);

  for (auto _ : state) {
    // Compile-time dispatch - explicit platform specification
    umpire::copy<umpire::resource::host_platform, umpire::resource::hip_platform>(
        static_cast<unsigned char*>(src_ptr),
        static_cast<unsigned char*>(dst_ptr),
        size);
    benchmark::DoNotOptimize(dst_ptr);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(size));

  src_allocator.deallocate(src_ptr);
  dst_allocator.deallocate(dst_ptr);
}

// HIP Device to Host
static void BM_CompileTimeDispatch_Copy_Hip_Host(benchmark::State& state) {
  auto& rm = umpire::ResourceManager::getInstance();

  auto src_allocator = rm.getAllocator("DEVICE");
  auto dst_allocator = rm.getAllocator("HOST");

  const std::size_t size = state.range(0);

  void* src_ptr = src_allocator.allocate(size);
  void* dst_ptr = dst_allocator.allocate(size);

  // Initialize source data on device
  unsigned char pattern = 0x22;
  umpire::memset(src_ptr, pattern, size);

  for (auto _ : state) {
    // Compile-time dispatch - explicit platform specification
    umpire::copy<umpire::resource::hip_platform, umpire::resource::host_platform>(
        static_cast<unsigned char*>(src_ptr),
        static_cast<unsigned char*>(dst_ptr),
        size);
    benchmark::DoNotOptimize(dst_ptr);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(size));

  src_allocator.deallocate(src_ptr);
  dst_allocator.deallocate(dst_ptr);
}
#endif // UMPIRE_ENABLE_HIP

//==============================================================================
// Benchmark Registration
//==============================================================================

// Host to Host benchmarks - all three approaches
BENCHMARK_CAPTURE(BM_ResourceManager_Copy, ResourceManager_Host_Host, std::string("HOST"), std::string("HOST"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_CAPTURE(BM_RuntimeDispatch_Copy, RuntimeDispatch_Host_Host, std::string("HOST"), std::string("HOST"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CompileTimeDispatch_Copy_Host_Host)
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

#if defined(UMPIRE_ENABLE_CUDA)
// Host to CUDA Device benchmarks
BENCHMARK_CAPTURE(BM_ResourceManager_Copy, ResourceManager_Host_Cuda, std::string("HOST"), std::string("DEVICE"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_CAPTURE(BM_RuntimeDispatch_Copy, RuntimeDispatch_Host_Cuda, std::string("HOST"), std::string("DEVICE"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CompileTimeDispatch_Copy_Host_Cuda)
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

// CUDA Device to Host benchmarks
BENCHMARK_CAPTURE(BM_ResourceManager_Copy, ResourceManager_Cuda_Host, std::string("DEVICE"), std::string("HOST"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_CAPTURE(BM_RuntimeDispatch_Copy, RuntimeDispatch_Cuda_Host, std::string("DEVICE"), std::string("HOST"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CompileTimeDispatch_Copy_Cuda_Host)
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

// CUDA Device to CUDA Device benchmarks
BENCHMARK_CAPTURE(BM_ResourceManager_Copy, ResourceManager_Cuda_Cuda, std::string("DEVICE"), std::string("DEVICE"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_CAPTURE(BM_RuntimeDispatch_Copy, RuntimeDispatch_Cuda_Cuda, std::string("DEVICE"), std::string("DEVICE"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CompileTimeDispatch_Copy_Cuda_Cuda)
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);
#endif // UMPIRE_ENABLE_CUDA

#if defined(UMPIRE_ENABLE_HIP)
// Host to HIP Device benchmarks
BENCHMARK_CAPTURE(BM_ResourceManager_Copy, ResourceManager_Host_Hip, std::string("HOST"), std::string("DEVICE"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_CAPTURE(BM_RuntimeDispatch_Copy, RuntimeDispatch_Host_Hip, std::string("HOST"), std::string("DEVICE"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CompileTimeDispatch_Copy_Host_Hip)
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

// HIP Device to Host benchmarks
BENCHMARK_CAPTURE(BM_ResourceManager_Copy, ResourceManager_Hip_Host, std::string("DEVICE"), std::string("HOST"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_CAPTURE(BM_RuntimeDispatch_Copy, RuntimeDispatch_Hip_Host, std::string("DEVICE"), std::string("HOST"))
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CompileTimeDispatch_Copy_Hip_Host)
    ->RangeMultiplier(MULTIPLIER)->Range(MIN_SIZE, MAX_SIZE)
    ->Unit(benchmark::kMicrosecond);
#endif // UMPIRE_ENABLE_HIP

BENCHMARK_MAIN();