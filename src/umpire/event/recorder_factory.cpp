//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-20, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "umpire/event/recorder_factory.hpp"

#include "umpire/config.hpp"

#ifndef WIN32
#include "umpire/event/quest_database.hpp"
#endif

#ifdef UMPIRE_ENABLE_SQLITE_EXPERIMENTAL
#include "umpire/event/sqlite_database.hpp"
#else
#include "umpire/event/json_file_store.hpp"
#endif // UMPIRE_ENABLE_SQLITE_EXPERIMENTAL

#include "umpire/util/Macros.hpp"
#include "umpire/util/io.hpp"

#if !defined(_MSC_VER)
#include <unistd.h> // getpid()
#else
#include <process.h>
#define getpid _getpid
#include <direct.h>
#endif

namespace umpire {
namespace event {

store_type& recorder_factory::get_recorder()
{
  static const std::string filename{
      util::make_unique_filename(util::get_io_output_dir(), util::get_io_output_basename(), getpid(), "stats")};

  // static quest_database db{"localhost", "9009", "db"};
  // static binary_file_database db{"test.bin"};
#ifdef UMPIRE_ENABLE_SQLITE_EXPERIMENTAL
  static sqlite_database db{filename};
#else
  static json_file_store db{filename};
#endif // UMPIRE_ENABLE_SQLITE_EXPERIMENTAL
  static event_store_recorder recorder(&db);

  static bool info_logged = false;
  if (!info_logged) {
    const char* replay_env = std::getenv("UMPIRE_REPLAY");
    const char* event_env = std::getenv("UMPIRE_EVENTS");
    if (replay_env != nullptr || event_env != nullptr) {
      UMPIRE_LOG(Info,
                 "Event recording enabled via environment variables. "
                 "Note: Variables must be set before program execution.");
    }
    info_logged = true;
  }

  return recorder;
}

} // namespace event
} // namespace umpire
