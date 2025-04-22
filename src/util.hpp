#pragma once

#include <algorithm>
#include <string>
#include <string_view>

struct string_hash {
  using is_transparent = void;

  auto operator()(std::string_view txt) const noexcept -> std::size_t {
    return std::hash<std::string_view>{}(txt);
  }

  auto operator()(const std::string &txt) const noexcept -> std::size_t {
    return std::hash<std::string_view>{}(txt);
  }

  auto operator()(const char *txt) const noexcept -> std::size_t {
    return std::hash<std::string_view>{}(txt);
  }
};
