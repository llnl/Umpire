//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_affinity_HPP
#define UMPIRE_affinity_HPP

#include <string>

namespace umpire {
namespace util {

// Return the socket id for the current process affinity mask.
bool get_socket_id_from_affinity(int& socket_id, std::string& reason);

// Return whether the current process affinity mask maps to one socket.
bool affinity_maps_to_single_socket(std::string& reason);

} // end namespace util
} // end namespace umpire

#endif // UMPIRE_affinity_HPP
