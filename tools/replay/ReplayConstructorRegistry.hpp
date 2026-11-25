//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef REPLAY_ReplayConstructorRegistry_HPP
#define REPLAY_ReplayConstructorRegistry_HPP

#if !defined(_MSC_VER)

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/json/json.hpp"

struct ReplayAllocatorSpec {
  std::string allocator_id;
  std::string name;
  std::string strategy;
  bool tracking{true};
  nlohmann::json args{};
};

struct ReplayAllocationState {
  std::string allocator_id;
  void* runtime_ptr{nullptr};
  std::size_t size{0};
};

struct ReplayContext {
  umpire::ResourceManager& resource_manager;
  std::unordered_map<std::string, umpire::Allocator> allocators{};
  std::unordered_map<std::string, ReplayAllocationState> allocations{};
  std::vector<std::string> allocator_order{};
};

class ReplayConstructorRegistry {
 public:
  using Factory = std::function<umpire::Allocator(const ReplayAllocatorSpec&, ReplayContext&)>;

  ReplayConstructorRegistry();

  umpire::Allocator construct(const ReplayAllocatorSpec& spec, ReplayContext& context) const;

 private:
  void registerFactory(const std::string& strategy, Factory factory);

  std::unordered_map<std::string, Factory> m_factories{};
};

#endif // !defined(_MSC_VER)
#endif // REPLAY_ReplayConstructorRegistry_HPP
