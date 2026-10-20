#include <stdio.h>
#include <math.h>
#include <iostream>

#include "camp/camp.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/strategy/QuickPool.hpp"
#include "umpire/strategy/ResourceAwarePool.hpp"

using namespace camp::resources;

#if defined(UMPIRE_ENABLE_CUDA)
using resource_type = Cuda;
#elif defined(UMPIRE_ENABLE_HIP)
using resource_type = Hip;
#endif

constexpr int ITER = 5;
constexpr int NUM = 2048;
const int NUM_PER_BLOCK = 256;
const int NUM_BLOCKS = NUM/NUM_PER_BLOCK;

using clock_value_t = long long;

__device__ clock_value_t my_clock()
{
  return clock64();
}

__device__ void sleep(clock_value_t sleep_cycles)
{
  clock_value_t start = my_clock();
  clock_value_t cycles_elapsed;
  do {
    cycles_elapsed = my_clock() - start;
  } while (cycles_elapsed < sleep_cycles);
}

__global__ void do_sleep()
{
  // Sleep in kernel in order to replicate data race
  sleep((clock_value_t)100000000);
}

__global__ void touch_data(double* data)
{
  int id = blockIdx.x * blockDim.x + threadIdx.x;

  if (id < NUM) {
    data[id] = id;
  }
}

__global__ void check_data(double* data)
{
  int id = blockIdx.x * blockDim.x + threadIdx.x;

  //Then error check that data[id] still == id
  if (id < NUM) {
    if (data[id] != id)
      data[id] = -1; 
  }
}

__global__ void touch_data_again(double* data)
{
  int id = blockIdx.x * blockDim.x + threadIdx.x;

  if (id < NUM) {
    data[id] = 8.76543210;
  }
}

void QuickPool_check(umpire::Allocator quick_pool)
{
  auto& rm = umpire::ResourceManager::getInstance();
  bool error{false};

  // Create hip streams
  auto s1 = resource_type().get_stream();
  auto s2 = resource_type().get_stream();

  double* a = static_cast<double*>(quick_pool.allocate(NUM * sizeof(double)));

  touch_data<<<NUM_BLOCKS, NUM_PER_BLOCK, 0, s1>>>(a);
  do_sleep<<<NUM_BLOCKS, NUM_PER_BLOCK, 0, s1>>>();
  check_data<<<NUM_BLOCKS, NUM_PER_BLOCK, 0, s1>>>(a);

  quick_pool.deallocate(a);
  a = static_cast<double*>(quick_pool.allocate(NUM * sizeof(double)));

  touch_data_again<<<NUM_BLOCKS, NUM_PER_BLOCK, 0, s2>>>(a);

  double* b = static_cast<double*>(quick_pool.allocate(NUM * sizeof(double)));
  resource_type().get_event().wait();
  rm.copy(b, a);
  b = static_cast<double*>(rm.move(b, rm.getAllocator("HOST")));

  for (int i = 0; i < NUM; i++) {
    if(b[i] == (-1)) {
      error = true;
      break;
    }
  }

  if (error) {
    std::cout << "Errors Found!" << std::endl;
  } else {
    std::cout << "Kernel succeeded! Expected result returned" << std::endl;
  }

  quick_pool.deallocate(a);
  rm.deallocate(b);
}

void ResourceAwarePool_check(umpire::Allocator rap_pool)
{
  // Create hip resources
  resource_type d1, d2;
  Resource r1{d1}, r2{d2};

  // ResourceAwarePool checks
  auto& rm = umpire::ResourceManager::getInstance();
  bool error{false};

  for(int i = 0; i < ITER; i++) {
    double* a = static_cast<double*>(rap_pool.allocate(NUM * sizeof(double), r1));

    touch_data<<<NUM_BLOCKS, NUM_PER_BLOCK, 0, d1.get_stream()>>>(a);
    do_sleep<<<NUM_BLOCKS, NUM_PER_BLOCK, 0, d1.get_stream()>>>();
    check_data<<<NUM_BLOCKS, NUM_PER_BLOCK, 0, d1.get_stream()>>>(a);

    rap_pool.deallocate(a, r1);
    a = static_cast<double*>(rap_pool.allocate(NUM * sizeof(double), r2));

    touch_data_again<<<NUM_BLOCKS, NUM_PER_BLOCK, 0, d2.get_stream()>>>(a);

    double* b = static_cast<double*>(rap_pool.allocate(NUM * sizeof(double), r2));
    r2.get_event().wait();
    rm.copy(b, a);
    b = static_cast<double*>(rm.move(b, rm.getAllocator("HOST")));

    for (int i = 0; i < NUM; i++) {
      if(b[i] == (-1)) {
        error = true;
        break;
      }
    }

    if (error) {
      std::cout << "Errors Found!" << std::endl;
    } else {
      std::cout << "Kernel succeeded! Expected result returned" << std::endl;
    }

    rap_pool.deallocate(a, r2);
    rm.deallocate(b);
    error = false; // reset to find any new errors in next iter
  }
}

int main(int, char**)
{
  auto& rm = umpire::ResourceManager::getInstance();
  auto quick_pool = rm.makeAllocator<umpire::strategy::QuickPool>("quick-pool", rm.getAllocator("UM"));
  auto rap_pool = rm.makeAllocator<umpire::strategy::ResourceAwarePool>("rap-pool", rm.getAllocator("UM"));

  std::cout<<"Checking QuickPool ...."<<std::endl;
  QuickPool_check(quick_pool);

  std::cout<<"Checking ResourceAwarePool ...."<<std::endl;
  ResourceAwarePool_check(rap_pool);

  std::cout<<"Done!"<<std::endl;
  return 0;
}
  
