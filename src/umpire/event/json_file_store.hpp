//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-20, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_json_file_store_HPP
#define UMPIRE_json_file_store_HPP

#include <stdio.h>

#include <string>
#include <vector>

#include "umpire/event/event_store.hpp"

namespace umpire {
namespace event {

struct event;
struct allocate;
struct named_allocate;
struct allocate_resource;
struct deallocate;
struct deallocate_resource;

class json_file_store : public event_store {
 public:
  json_file_store(const std::string& filename, bool read_only = false);
  ~json_file_store();

  virtual void insert(const event& e);
  virtual void insert(const allocate& e);
  virtual void insert(const named_allocate& e);
  virtual void insert(const allocate_resource& e);
  virtual void insert(const deallocate& e);
  virtual void insert(const deallocate_resource& e);

  virtual std::vector<event> get_events();

 private:
  FILE* m_fstream{nullptr};
  std::string m_filename;
  bool m_read_only;
};

} // namespace event
} // namespace umpire
#endif // UMPIRE_json_file_store_HPP
