//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_io_HPP
#define UMPIRE_io_HPP

#include <string>

namespace umpire {
namespace util {

/*!
 * \brief Generate a unique filename with format: base_dir/name.pid.id.extension
 * The id is incremented until a non-existing file is found.
 */
std::string make_unique_filename(const std::string& base_dir, const std::string& name, const int pid,
                                 const std::string& extension);

/*!
 * \brief Check if a file exists
 */
bool file_exists(const std::string& file);

/*!
 * \brief Check if a directory exists
 */
bool directory_exists(const std::string& file);

/*!
 * \brief Create the given output directory if it does not exist.
 *
 * Under MPI, only rank 0 creates the directory and all ranks synchronize
 * before returning, so shared-filesystem creation is coordinated.
 *
 * \throw umpire::runtime_error if the directory cannot be created, if the
 *        path exists but is not a directory, or if called before MPI has
 *        been initialized.
 */
void make_io_dir(const std::string& io_dir);

/*!
 * \brief Get the output directory from UMPIRE_OUTPUT_DIR environment variable
 * Returns "./" if not set
 */
const std::string& get_io_output_dir();

/*!
 * \brief Get the output basename from UMPIRE_OUTPUT_BASENAME environment variable
 * Returns "umpire" if not set
 */
const std::string& get_io_output_basename();

} // end namespace util
} // end namespace umpire

#endif // UMPIRE_io_HPP
