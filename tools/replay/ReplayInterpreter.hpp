//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef REPLAY_ReplayInterpreter_HPP
#define REPLAY_ReplayInterpreter_HPP

#if !defined(_MSC_VER)

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ReplayConstructorRegistry.hpp"
#include "ReplayOptions.hpp"
#include "umpire/json/json.hpp"

class ReplayInterpreter {
 public:
  explicit ReplayInterpreter(const ReplayOptions& options);

  void buildOperations();
  void runOperations();
  bool compareOperations(ReplayInterpreter& rh);

 private:
  using StatSeries = std::map<std::string, std::vector<std::pair<std::size_t, std::size_t>>>;

  ReplayAllocatorSpec parseAllocatorSpec(const nlohmann::json& command) const;
  nlohmann::json normalizeHeader(nlohmann::json header) const;
  std::vector<nlohmann::json> normalizeCommands() const;
  void appendStats(ReplayContext& context, std::size_t seq);
  void dumpStats() const;
  void printStats(ReplayContext& context) const;

  ReplayOptions m_options;
  nlohmann::json m_header{};
  std::vector<nlohmann::json> m_commands{};
  ReplayConstructorRegistry m_registry{};
  StatSeries m_stat_series{};
};

#endif // !defined(_MSC_VER)
#endif // REPLAY_ReplayInterpreter_HPP
