//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#if !defined(_MSC_VER)

#include "ReplayInterpreter.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "umpire/util/error.hpp"

#if !defined(_MSC_VER)
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif

namespace {

nlohmann::json readJsonLine(const std::string& line, const std::string& filename, std::size_t line_number)
{
  try {
    return nlohmann::json::parse(line);
  } catch (const std::exception& ex) {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Failed to parse replay JSON at {}:{} ({})", filename, line_number, ex.what()));
  }
}

std::string requiredString(const nlohmann::json& command, const char* key)
{
  auto found = command.find(key);
  if (found == command.end() || !found->is_string()) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("Replay command is missing required string field {}", key));
  }
  return found->get<std::string>();
}

std::size_t requiredSize(const nlohmann::json& command, const char* key)
{
  auto found = command.find(key);
  if (found == command.end() || !found->is_number_unsigned()) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("Replay command is missing required numeric field {}", key));
  }
  return found->get<std::size_t>();
}

std::string commandStatus(const nlohmann::json& command)
{
  auto found = command.find("status");
  if (found == command.end()) {
    return "committed";
  }

  if (!found->is_string()) {
    UMPIRE_ERROR(umpire::runtime_error, "Replay command has a non-string status field");
  }

  const auto status = found->get<std::string>();
  if (status != "pending" && status != "committed") {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("Replay command has unsupported status {}", status));
  }

  return status;
}

nlohmann::json normalizedCommand(nlohmann::json command)
{
  command["status"] = commandStatus(command);
  return command;
}

nlohmann::json stripStatus(nlohmann::json command)
{
  command.erase("status");
  return command;
}

std::string commandKey(const nlohmann::json& command)
{
  const auto op = requiredString(command, "op");

  if (op == "make_allocator") {
    return op + ":" + requiredString(command, "allocator_id") + ":" + std::to_string(requiredSize(command, "seq"));
  }

  if (op == "allocate" || op == "deallocate") {
    return op + ":" + requiredString(command, "allocation_id");
  }

  UMPIRE_ERROR(umpire::runtime_error, fmt::format("Unsupported replay operation {}", op));
}

} // namespace

ReplayInterpreter::ReplayInterpreter(const ReplayOptions& options) : m_options(options) {}

void ReplayInterpreter::buildOperations()
{
  std::ifstream input{m_options.input_file};
  if (!input) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("Unable to open replay file {}", m_options.input_file));
  }

  m_header = nlohmann::json{};
  m_commands.clear();
  std::string line;
  std::size_t line_number{0};

  while (std::getline(input, line)) {
    ++line_number;
    if (line.empty()) {
      continue;
    }

    auto json_line = readJsonLine(line, m_options.input_file, line_number);
    if (line_number == 1) {
      m_header = json_line;
    } else {
      m_commands.emplace_back(std::move(json_line));
    }
  }

  if (m_header.empty()) {
    UMPIRE_ERROR(umpire::runtime_error, fmt::format("Replay file {} is empty", m_options.input_file));
  }

  if (m_header.value("kind", "") != "umpire_replay" || m_header.value("schema", "") != "v2") {
    UMPIRE_ERROR(umpire::runtime_error,
                 fmt::format("Replay file {} is not a supported umpire replay v2 trace", m_options.input_file));
  }
}

ReplayAllocatorSpec ReplayInterpreter::parseAllocatorSpec(const nlohmann::json& command) const
{
  ReplayAllocatorSpec spec;
  spec.allocator_id = requiredString(command, "allocator_id");
  spec.name = requiredString(command, "name");
  spec.strategy = requiredString(command, "strategy");
  spec.tracking = command.value("tracking", true);
  spec.args = command.value("args", nlohmann::json::object());
  return spec;
}

nlohmann::json ReplayInterpreter::normalizeHeader(nlohmann::json header) const
{
  if (header.contains("process") && header["process"].is_object()) {
    header["process"].erase("pid");
  }
  return header;
}

std::vector<nlohmann::json> ReplayInterpreter::normalizeCommands() const
{
  std::vector<nlohmann::json> normalized;
  normalized.reserve(m_commands.size());

  std::unordered_map<std::string, nlohmann::json> pending_commands;

  for (const auto& raw_command : m_commands) {
    const bool has_explicit_status = raw_command.contains("status");
    auto command = normalizedCommand(raw_command);
    const auto status = requiredString(command, "status");
    const auto key = commandKey(command);

    if (status == "pending") {
      auto inserted = pending_commands.emplace(key, command);
      if (!inserted.second) {
        UMPIRE_ERROR(umpire::runtime_error,
                     fmt::format("Replay command {} has duplicate pending lifecycle state", key));
      }
    } else {
      auto pending = pending_commands.find(key);
      if (pending != pending_commands.end()) {
        if (stripStatus(pending->second) != stripStatus(command)) {
          UMPIRE_ERROR(umpire::runtime_error,
                       fmt::format("Replay command {} has mismatched pending and committed payloads", key));
        }
        pending_commands.erase(pending);
      } else if (has_explicit_status) {
        UMPIRE_ERROR(umpire::runtime_error,
                     fmt::format("Replay command {} has committed lifecycle state without a pending record", key));
      }
    }

    normalized.emplace_back(std::move(command));
  }

  return normalized;
}

bool ReplayInterpreter::compareOperations(ReplayInterpreter& rh)
{
  if (m_header.empty()) {
    buildOperations();
  }
  if (rh.m_header.empty()) {
    rh.buildOperations();
  }

  if (normalizeHeader(m_header) != normalizeHeader(rh.m_header)) {
    return false;
  }

  return normalizeCommands() == rh.normalizeCommands();
}

void ReplayInterpreter::appendStats(ReplayContext& context, std::size_t seq)
{
  for (const auto& allocator_id : context.allocator_order) {
    auto found = context.allocators.find(allocator_id);
    if (found == context.allocators.end()) {
      continue;
    }

    auto allocator = found->second;
    const auto& allocator_name = allocator.getName();

    m_stat_series[allocator_name + " current_size"].push_back({seq, allocator.getCurrentSize()});
    m_stat_series[allocator_name + " actual_size"].push_back({seq, allocator.getActualSize()});
    m_stat_series[allocator_name + " high_watermark"].push_back({seq, allocator.getHighWatermark()});
    m_stat_series[allocator_name + " allocation_count"].push_back({seq, allocator.getAllocationCount()});
  }
}

void ReplayInterpreter::dumpStats() const
{
  std::ofstream file{"replay" + std::to_string(getpid()) + ".ult"};
  if (!file) {
    UMPIRE_ERROR(umpire::runtime_error, "Unable to create replay ULT output file");
  }

  for (const auto& stat_series : m_stat_series) {
    file << "# " << stat_series.first << "\n";
    for (const auto& entry : stat_series.second) {
      file << entry.first << " " << entry.second << "\n";
    }
  }
}

void ReplayInterpreter::printStats(ReplayContext& context) const
{
  const int name_width{40};
  const int num_width{16};

  std::cout << std::setw(name_width) << std::left << "Filename" << std::setw(name_width) << std::left << "Allocator"
            << std::setw(num_width) << std::left << "Current Size" << std::setw(num_width) << std::left << "Actual Size"
            << std::setw(num_width) << std::left << "High Watermark" << "\n";

  for (const auto& allocator_id : context.allocator_order) {
    auto found = context.allocators.find(allocator_id);
    if (found == context.allocators.end()) {
      continue;
    }

    auto allocator = found->second;
    if (allocator.getHighWatermark() == 0) {
      continue;
    }

    std::cout << std::setw(name_width) << std::left << m_options.input_file << std::setw(name_width) << std::left
              << allocator.getName() << std::setw(num_width) << std::left << allocator.getCurrentSize()
              << std::setw(num_width) << std::left << allocator.getActualSize() << std::setw(num_width) << std::left
              << allocator.getHighWatermark() << "\n";
  }
}

void ReplayInterpreter::runOperations()
{
  if (m_header.empty()) {
    buildOperations();
  }

  ReplayContext context{umpire::ResourceManager::getInstance()};
  m_stat_series.clear();
  const auto normalized_commands = normalizeCommands();

  for (const auto& command : normalized_commands) {
    const auto op = requiredString(command, "op");
    const auto status = requiredString(command, "status");
    const auto seq = requiredSize(command, "seq");

    if (status == "pending") {
      continue;
    }

    if (op == "make_allocator") {
      auto spec = parseAllocatorSpec(command);
      auto allocator = m_registry.construct(spec, context);
      context.allocators[spec.allocator_id] = allocator;
      context.allocator_order.push_back(spec.allocator_id);
    } else if (op == "allocate") {
      const auto allocator_id = requiredString(command, "allocator_id");
      const auto allocation_id = requiredString(command, "allocation_id");
      const auto size = requiredSize(command, "size");

      auto allocator_it = context.allocators.find(allocator_id);
      if (allocator_it == context.allocators.end()) {
        UMPIRE_ERROR(umpire::runtime_error,
                     fmt::format("Replay allocate references unknown allocator {}", allocator_id));
      }

      void* ptr = allocator_it->second.allocate(size);
      context.allocations[allocation_id] = ReplayAllocationState{allocator_id, ptr, size};
    } else if (op == "deallocate") {
      const auto allocator_id = requiredString(command, "allocator_id");
      const auto allocation_id = requiredString(command, "allocation_id");

      auto allocation_it = context.allocations.find(allocation_id);
      if (allocation_it == context.allocations.end()) {
        UMPIRE_ERROR(umpire::runtime_error,
                     fmt::format("Replay deallocate references unknown allocation {}", allocation_id));
      }

      if (allocation_it->second.allocator_id != allocator_id) {
        UMPIRE_ERROR(umpire::runtime_error,
                     fmt::format("Replay deallocate allocator mismatch for allocation {}", allocation_id));
      }

      auto allocator_it = context.allocators.find(allocator_id);
      if (allocator_it == context.allocators.end()) {
        UMPIRE_ERROR(umpire::runtime_error,
                     fmt::format("Replay deallocate references unknown allocator {}", allocator_id));
      }

      allocator_it->second.deallocate(allocation_it->second.runtime_ptr);
      context.allocations.erase(allocation_it);
    } else {
      UMPIRE_ERROR(umpire::runtime_error, fmt::format("Unsupported replay operation {}", op));
    }

    if (m_options.dump_statistics || m_options.track_stats) {
      appendStats(context, seq);
    }
  }

  if (m_options.dump_statistics) {
    dumpStats();
  }

  if (m_options.track_stats && !m_options.quiet) {
    printStats(context);
  }
}

#endif // !defined(_MSC_VER)
