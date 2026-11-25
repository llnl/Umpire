//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////

#include "umpire/replay/Replay.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include "umpire/config.hpp"
#include "umpire/util/MPI.hpp"
#include "umpire/util/io.hpp"

#if !defined(_MSC_VER)
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif

namespace umpire {
namespace replay {

namespace {

thread_local int g_nested_resource_suppression_depth{0};

const char* statusString(ReplayCommandStatus status) noexcept
{
  switch (status) {
    case ReplayCommandStatus::pending:
      return "pending";
    case ReplayCommandStatus::committed:
      return "committed";
  }

  return "committed";
}

struct AllocationInfo {
  std::string allocation_id;
  std::string allocator_id;
};

class ReplayRecorder {
 public:
  static ReplayRecorder& getInstance()
  {
    static ReplayRecorder recorder;
    return recorder;
  }

  std::string resolveAllocatorId(strategy::AllocationStrategy* allocator)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto found = m_allocator_ids.find(allocator);
    if (found == m_allocator_ids.end()) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Replay allocator reference {} was used before it was recorded", fmt::ptr(allocator)));
    }
    return found->second;
  }

  ReplayMakeAllocatorToken beginMakeAllocator(const std::string& name, bool tracking, const std::string& strategy_name,
                                              const json& args)
  {
    if (g_nested_resource_suppression_depth > 0) {
      return {};
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    ReplayMakeAllocatorToken token;
    token.active = true;
    token.seq = nextSequence();
    token.allocator_id = nextAllocatorId();
    token.name = name;
    token.strategy_name = strategy_name;
    token.tracking = tracking;
    token.args = args;

    writeCommand(json{{"op", "make_allocator"},
                      {"seq", token.seq},
                      {"allocator_id", token.allocator_id},
                      {"name", token.name},
                      {"strategy", token.strategy_name},
                      {"tracking", token.tracking},
                      {"args", token.args},
                      {"status", statusString(ReplayCommandStatus::pending)}});

    return token;
  }

  void commitMakeAllocator(strategy::AllocationStrategy* allocator, const ReplayMakeAllocatorToken& token)
  {
    if (!token.active) {
      return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_allocator_ids[allocator] = token.allocator_id;

    writeCommand(json{{"op", "make_allocator"},
                      {"seq", token.seq},
                      {"allocator_id", token.allocator_id},
                      {"name", token.name},
                      {"strategy", token.strategy_name},
                      {"tracking", token.tracking},
                      {"args", token.args},
                      {"status", statusString(ReplayCommandStatus::committed)}});
  }

  ReplayAllocateToken beginAllocate(strategy::AllocationStrategy* allocator, std::size_t size)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    ReplayAllocateToken token;
    token.active = true;
    token.seq = nextSequence();
    token.allocator_id = resolveAllocatorIdUnlocked(allocator);
    token.allocation_id = nextAllocationId();
    token.size = size;

    writeCommand(json{{"op", "allocate"},
                      {"seq", token.seq},
                      {"allocator_id", token.allocator_id},
                      {"allocation_id", token.allocation_id},
                      {"size", token.size},
                      {"status", statusString(ReplayCommandStatus::pending)}});

    return token;
  }

  void commitAllocate(void* ptr, const ReplayAllocateToken& token)
  {
    if (!token.active) {
      return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_allocations[ptr] = AllocationInfo{token.allocation_id, token.allocator_id};

    writeCommand(json{{"op", "allocate"},
                      {"seq", token.seq},
                      {"allocator_id", token.allocator_id},
                      {"allocation_id", token.allocation_id},
                      {"size", token.size},
                      {"status", statusString(ReplayCommandStatus::committed)}});
  }

  ReplayDeallocateToken beginDeallocate(strategy::AllocationStrategy* allocator, void* ptr)
  {
    if (!ptr) {
      return {};
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto allocator_id = resolveAllocatorIdUnlocked(allocator);
    auto found = m_allocations.find(ptr);
    if (found == m_allocations.end()) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Replay allocation reference {} was deallocated before it was recorded", fmt::ptr(ptr)));
    }

    if (found->second.allocator_id != allocator_id) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Replay deallocate allocator mismatch for allocation {}", found->second.allocation_id));
    }

    ReplayDeallocateToken token;
    token.active = true;
    token.seq = nextSequence();
    token.allocator_id = allocator_id;
    token.allocation_id = found->second.allocation_id;

    writeCommand(json{{"op", "deallocate"},
                      {"seq", token.seq},
                      {"allocator_id", token.allocator_id},
                      {"allocation_id", token.allocation_id},
                      {"status", statusString(ReplayCommandStatus::pending)}});

    return token;
  }

  void commitDeallocate(const ReplayDeallocateToken& token)
  {
    if (!token.active) {
      return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto found =
        std::find_if(m_allocations.begin(), m_allocations.end(), [&](const auto& entry) -> bool {
          return entry.second.allocation_id == token.allocation_id && entry.second.allocator_id == token.allocator_id;
        });

    if (found == m_allocations.end()) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Replay allocation {} was committed for deallocation before it was recorded",
                               token.allocation_id));
    }

    writeCommand(json{{"op", "deallocate"},
                      {"seq", token.seq},
                      {"allocator_id", token.allocator_id},
                      {"allocation_id", token.allocation_id},
                      {"status", statusString(ReplayCommandStatus::committed)}});

    m_allocations.erase(found);
  }

 private:
  ReplayRecorder() = default;

  static std::string versionString()
  {
    std::ostringstream version;
    version << UMPIRE_VERSION_MAJOR << "." << UMPIRE_VERSION_MINOR << "." << UMPIRE_VERSION_PATCH;
    if (std::string{UMPIRE_VERSION_RC} != "") {
      version << "-" << UMPIRE_VERSION_RC;
    }
    return version.str();
  }

  void ensureFile()
  {
    if (m_file.is_open()) {
      return;
    }

    m_filename = util::make_unique_filename(util::get_io_output_dir(), util::get_io_output_basename(), getpid(), "stats");
    m_file.open(m_filename);
    if (!m_file) {
      UMPIRE_ERROR(runtime_error, fmt::format("Failed to open replay output file {}", m_filename));
    }
  }

  void ensureHeader()
  {
    if (m_header_written) {
      return;
    }

    ensureFile();
    const int rank = util::MPI::isInitialized() ? util::MPI::getRank() : 0;
    json header{{"kind", "umpire_replay"},
                {"schema", "v2"},
                {"process", {{"pid", getpid()}, {"rank", rank}}},
                {"umpire_version", versionString()}};
    m_file << header.dump() << '\n';
    m_file.flush();
    m_header_written = true;
  }

  std::size_t nextSequence()
  {
    return ++m_sequence;
  }

  std::string nextAllocatorId()
  {
    return "a" + std::to_string(++m_next_allocator_id);
  }

  std::string nextAllocationId()
  {
    return "m" + std::to_string(++m_next_allocation_id);
  }

  std::string resolveAllocatorIdUnlocked(strategy::AllocationStrategy* allocator)
  {
    auto found = m_allocator_ids.find(allocator);
    if (found == m_allocator_ids.end()) {
      UMPIRE_ERROR(runtime_error,
                   fmt::format("Replay allocator reference {} was used before it was recorded", fmt::ptr(allocator)));
    }
    return found->second;
  }

  void writeCommand(const json& command)
  {
    ensureHeader();
    m_file << command.dump() << '\n';
    m_file.flush();
  }

  std::mutex m_mutex{};
  std::ofstream m_file{};
  std::string m_filename{};
  bool m_header_written{false};
  std::size_t m_sequence{0};
  std::size_t m_next_allocator_id{0};
  std::size_t m_next_allocation_id{0};
  std::unordered_map<strategy::AllocationStrategy*, std::string> m_allocator_ids{};
  std::unordered_map<void*, AllocationInfo> m_allocations{};
};

MemoryResourceTraits::shared_scope sharedScopeFromString(const std::string& scope)
{
  if (scope == "node") {
    return MemoryResourceTraits::shared_scope::node;
  }
  if (scope == "socket") {
    return MemoryResourceTraits::shared_scope::socket;
  }
  return MemoryResourceTraits::shared_scope::unknown;
}

MemoryResourceTraits::optimized_for optimizedForFromString(const std::string& value)
{
  if (value == "latency") {
    return MemoryResourceTraits::optimized_for::latency;
  }
  if (value == "bandwidth") {
    return MemoryResourceTraits::optimized_for::bandwidth;
  }
  if (value == "access") {
    return MemoryResourceTraits::optimized_for::access;
  }
  return MemoryResourceTraits::optimized_for::any;
}

MemoryResourceTraits::vendor_type vendorFromString(const std::string& vendor)
{
  if (vendor == "amd") {
    return MemoryResourceTraits::vendor_type::amd;
  }
  if (vendor == "ibm") {
    return MemoryResourceTraits::vendor_type::ibm;
  }
  if (vendor == "intel") {
    return MemoryResourceTraits::vendor_type::intel;
  }
  if (vendor == "nvidia") {
    return MemoryResourceTraits::vendor_type::nvidia;
  }
  return MemoryResourceTraits::vendor_type::unknown;
}

MemoryResourceTraits::memory_type memoryTypeFromString(const std::string& type)
{
  if (type == "ddr") {
    return MemoryResourceTraits::memory_type::ddr;
  }
  if (type == "gddr") {
    return MemoryResourceTraits::memory_type::gddr;
  }
  if (type == "hbm") {
    return MemoryResourceTraits::memory_type::hbm;
  }
  if (type == "nvme") {
    return MemoryResourceTraits::memory_type::nvme;
  }
  return MemoryResourceTraits::memory_type::unknown;
}

MemoryResourceTraits::resource_type resourceTypeFromString(const std::string& resource)
{
  if (resource == "host") {
    return MemoryResourceTraits::resource_type::host;
  }
  if (resource == "device") {
    return MemoryResourceTraits::resource_type::device;
  }
  if (resource == "device_const") {
    return MemoryResourceTraits::resource_type::device_const;
  }
  if (resource == "pinned") {
    return MemoryResourceTraits::resource_type::pinned;
  }
  if (resource == "um") {
    return MemoryResourceTraits::resource_type::um;
  }
  if (resource == "file") {
    return MemoryResourceTraits::resource_type::file;
  }
  if (resource == "shared") {
    return MemoryResourceTraits::resource_type::shared;
  }
  return MemoryResourceTraits::resource_type::unknown;
}

MemoryResourceTraits::granularity_type granularityFromString(const std::string& granularity)
{
  if (granularity == "fine_grained") {
    return MemoryResourceTraits::granularity_type::fine_grained;
  }
  if (granularity == "coarse_grained") {
    return MemoryResourceTraits::granularity_type::coarse_grained;
  }
  return MemoryResourceTraits::granularity_type::unknown;
}

} // namespace

ScopedNestedReplaySuppression::ScopedNestedReplaySuppression(bool enabled) : m_enabled(enabled)
{
  if (m_enabled) {
    ++g_nested_resource_suppression_depth;
  }
}

ScopedNestedReplaySuppression::~ScopedNestedReplaySuppression()
{
  if (m_enabled) {
    --g_nested_resource_suppression_depth;
  }
}

bool is_enabled() noexcept
{
  static const bool enabled = std::getenv("UMPIRE_REPLAY") != nullptr;
  return enabled;
}

std::string resolve_allocator_id(const Allocator& allocator)
{
  return resolve_allocator_id(allocator.getAllocationStrategy());
}

std::string resolve_allocator_id(strategy::AllocationStrategy* allocator)
{
  return ReplayRecorder::getInstance().resolveAllocatorId(allocator);
}

ReplayMakeAllocatorToken begin_make_allocator(const std::string& name, bool tracking, const std::string& strategy_name,
                                              const json& args)
{
  if (!is_enabled()) {
    return {};
  }

  return ReplayRecorder::getInstance().beginMakeAllocator(name, tracking, strategy_name, args);
}

void commit_make_allocator(strategy::AllocationStrategy* allocator, const ReplayMakeAllocatorToken& token)
{
  if (!is_enabled()) {
    return;
  }

  ReplayRecorder::getInstance().commitMakeAllocator(allocator, token);
}

ReplayAllocateToken begin_allocate(strategy::AllocationStrategy* allocator, std::size_t size)
{
  if (!is_enabled()) {
    return {};
  }

  return ReplayRecorder::getInstance().beginAllocate(allocator, size);
}

void commit_allocate(void* ptr, const ReplayAllocateToken& token)
{
  if (!is_enabled()) {
    return;
  }

  ReplayRecorder::getInstance().commitAllocate(ptr, token);
}

ReplayDeallocateToken begin_deallocate(strategy::AllocationStrategy* allocator, void* ptr)
{
  if (!is_enabled()) {
    return {};
  }

  return ReplayRecorder::getInstance().beginDeallocate(allocator, ptr);
}

void commit_deallocate(const ReplayDeallocateToken& token)
{
  if (!is_enabled()) {
    return;
  }

  ReplayRecorder::getInstance().commitDeallocate(token);
}

json serialize_memory_resource_args(const std::string& resource_name, const MemoryResourceTraits& traits)
{
  return json{{"resource_name", resource_name},
              {"traits",
               {{"unified", traits.unified},
                {"ipc", traits.ipc},
                {"size", traits.size},
                {"vendor", to_string(traits.vendor)},
                {"kind", to_string(traits.kind)},
                {"used_for", to_string(traits.used_for)},
                {"resource", to_string(traits.resource)},
                {"scope", to_string(traits.scope)},
                {"granularity", to_string(traits.granularity)},
                {"tracking", traits.tracking}}}};
}

MemoryResourceTraits deserialize_memory_resource_traits(const json& traits_json)
{
  MemoryResourceTraits traits;
  traits.unified = traits_json.value("unified", false);
  traits.ipc = traits_json.value("ipc", false);
  traits.size = traits_json.value("size", std::size_t{0});
  traits.vendor = vendorFromString(traits_json.value("vendor", "unknown"));
  traits.kind = memoryTypeFromString(traits_json.value("kind", "unknown"));
  traits.used_for = optimizedForFromString(traits_json.value("used_for", "any"));
  traits.resource = resourceTypeFromString(traits_json.value("resource", "unknown"));
  traits.scope = sharedScopeFromString(traits_json.value("scope", "unknown"));
  traits.granularity = granularityFromString(traits_json.value("granularity", "unknown"));
  traits.tracking = traits_json.value("tracking", true);
  return traits;
}

} // namespace replay
} // namespace umpire
