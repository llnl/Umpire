//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#ifndef UMPIRE_PoolCoalesceHeuristic_HPP
#define UMPIRE_PoolCoalesceHeuristic_HPP

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace umpire {

namespace strategy {

template <typename T>
class PoolCoalesceHeuristic {
 public:
  using Function = std::function<std::size_t(const T&)>;

  enum class Kind { opaque, percent_releasable, percent_releasable_hwm, blocks_releasable, blocks_releasable_hwm };

  PoolCoalesceHeuristic() = default;

  PoolCoalesceHeuristic(Function function) : m_function(std::move(function)) {}

  template <typename Callable,
            typename = std::enable_if_t<!std::is_same<std::decay_t<Callable>, PoolCoalesceHeuristic>::value>>
  PoolCoalesceHeuristic(Callable&& function) : m_function(std::forward<Callable>(function))
  {
  }

  static PoolCoalesceHeuristic known(Function function, Kind kind, std::size_t parameter)
  {
    PoolCoalesceHeuristic heuristic{std::move(function)};
    heuristic.m_kind = kind;
    heuristic.m_parameter = parameter;
    return heuristic;
  }

  std::size_t operator()(const T& pool) const
  {
    return m_function ? m_function(pool) : 0;
  }

  explicit operator bool() const noexcept
  {
    return static_cast<bool>(m_function);
  }

  Kind kind() const noexcept
  {
    return m_kind;
  }

  std::size_t parameter() const noexcept
  {
    return m_parameter;
  }

  bool is_known() const noexcept
  {
    return m_kind != Kind::opaque;
  }

  std::string kind_string() const
  {
    switch (m_kind) {
      case Kind::percent_releasable:
        return "percent_releasable";
      case Kind::percent_releasable_hwm:
        return "percent_releasable_hwm";
      case Kind::blocks_releasable:
        return "blocks_releasable";
      case Kind::blocks_releasable_hwm:
        return "blocks_releasable_hwm";
      case Kind::opaque:
      default:
        return "opaque";
    }
  }

 private:
  Function m_function{};
  Kind m_kind{Kind::opaque};
  std::size_t m_parameter{0};
};

} // end of namespace strategy
} // end namespace umpire

#endif // UMPIRE_PoolCoalesceHeuristic_HPP
