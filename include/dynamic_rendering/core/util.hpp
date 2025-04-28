#pragma once

#include <algorithm>
#include <string>
#include <string_view>

struct string_hash
{
  using is_transparent = void;

  auto operator()(std::string_view txt) const noexcept -> std::size_t
  {
    return std::hash<std::string_view>{}(txt);
  }

  auto operator()(const std::string& txt) const noexcept -> std::size_t
  {
    return std::hash<std::string_view>{}(txt);
  }

  auto operator()(const char* txt) const noexcept -> std::size_t
  {
    return std::hash<std::string_view>{}(txt);
  }
};

// WHat could i call type aliases for unordered_map and unordered_set with
// custom hash functions?

using string_hash_set =
  std::unordered_set<std::string, string_hash, std::equal_to<>>;
template<typename T>
using string_hash_map =
  std::unordered_map<std::string, T, string_hash, std::equal_to<>>;

template<typename T>
class Badge
{
  friend T;
  Badge() {}
};