//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "umpire/util/MPI.hpp"

#include "umpire/config.hpp"
#include "umpire/event/event.hpp"
#include "umpire/util/Macros.hpp"
#include "umpire/util/error.hpp"

#if defined(UMPIRE_ENABLE_MPI)
#include "mpi.h"
#endif

namespace umpire {
namespace util {

int MPI::s_rank = -1;
int MPI::s_world_size = -1;
bool MPI::s_initialized = false;
int MPI::s_mpi_init_called = 0;
#if defined(UMPIRE_ENABLE_MPI)
MPI_Comm MPI::s_communicator = MPI_COMM_NULL;
#endif

#if defined(UMPIRE_ENABLE_MPI)
std::string get_mpi_error_message(int error_code)
{
  char buffer[MPI_MAX_ERROR_STRING];
  int length{0};
  const int status = MPI_Error_string(error_code, buffer, &length);

  if (status != MPI_SUCCESS) {
    return fmt::format("MPI error code {} (MPI_Error_string failed with code {})", error_code, status);
  }

  return std::string{buffer, static_cast<std::size_t>(length)};
}

void check_mpi_call(int error_code, const char* call_name)
{
  if (error_code != MPI_SUCCESS) {
    UMPIRE_ERROR(runtime_error, fmt::format("{} failed: {}", call_name, get_mpi_error_message(error_code)));
  }
}
#endif

void MPI::initialize(
#if defined(UMPIRE_ENABLE_MPI)
    MPI_Comm comm
#endif
)
{
  if (!s_initialized) {
#if !defined(UMPIRE_ENABLE_MPI)
    s_rank = 0;
    s_world_size = 1;
    s_mpi_init_called = 1;
    s_initialized = true;
#else
    MPI_Initialized(&s_mpi_init_called);

    if (s_mpi_init_called) {
      s_communicator = comm;
      MPI_Comm_rank(s_communicator, &s_rank);
      MPI_Comm_size(s_communicator, &s_world_size);
      s_initialized = true;
    }

#endif
  } else {
    UMPIRE_ERROR(runtime_error, "umpire::MPI already initialized, cannot call initialize() again!");
  }
}

void MPI::finalize()
{
  if (s_initialized) {
#if !defined(UMPIRE_ENABLE_MPI)
    s_rank = -1;
    s_world_size = -1;
#endif
  } else {
    UMPIRE_ERROR(runtime_error, "Cannot call MPI::finalize() when umpire::MPI not initialiazed");
  }
}

int MPI::getRank()
{
  if (!s_initialized) {
    UMPIRE_LOG(Warning, fmt::format("umpire::MPI not initialized, returning rank={}", s_rank));
  }

  return s_rank;
}

int MPI::getSize()
{
  if (!s_initialized) {
    UMPIRE_LOG(Warning, fmt::format("umpire::MPI not initialized, returning size={}", s_world_size));
  }

  return s_world_size;
}

void MPI::sync()
{
  if (s_initialized) {
#if defined(UMPIRE_ENABLE_MPI)
    MPI_Barrier(s_communicator);
#endif
  } else {
    UMPIRE_ERROR(runtime_error, "Cannot call MPI::sync() before umpire::MPI is initialized");
  }
}

void MPI::logMpiInfo()
{
  if (s_initialized) {
#if defined(UMPIRE_ENABLE_MPI)
    UMPIRE_LOG(Info, "MPI rank: " << s_rank);
    UMPIRE_LOG(Info, "MPI comm size: " << s_world_size);

    umpire::event::record([&](auto& event) {
      event.name("mpi").category(event::category::metadata).arg("world_size", s_world_size).arg("rank", s_rank);
    });
#endif
  }
}

bool MPI::isInitialized()
{
  return s_initialized;
}

#if defined(UMPIRE_ENABLE_MPI)
MPI_Comm MPI::getCommunicator()
{
  if (s_initialized && s_communicator != MPI_COMM_NULL) {
    return s_communicator;
  }
  return MPI_COMM_WORLD;
}
#endif

} // end of namespace util
} // end of namespace umpire
