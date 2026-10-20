//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_node_memory_HPP
#define UMPIRE_node_memory_HPP

namespace umpire {
namespace util {

/*!
 * \brief Get the amount of available memory for the compute node in MiB.
 *
 * This function reads /proc/meminfo on Linux systems to determine the
 * available memory. On non-Linux systems or if the information cannot
 * be read, it returns the provided default value.
 *
 * \param default_MiB The default value to return if available memory
 *                    cannot be determined (default: -1.0)
 *
 * \return The available memory in MiB, or default_MiB if unavailable
 */
double get_node_available_memory(double default_MiB = -1.0);

} // end namespace util
} // end namespace umpire

#endif // UMPIRE_node_memory_HPP
