#pragma once

#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>

namespace VkCVar {

using CVarValue = std::variant<int, float, bool, std::string>;

template<typename T>
concept CVarCompatible = requires {
  requires((std::is_same_v<T, std::variant_alternative_t<0, CVarValue>> ||
            std::is_same_v<T, std::variant_alternative_t<1, CVarValue>> ||
            std::is_same_v<T, std::variant_alternative_t<2, CVarValue>> ||
            std::is_same_v<T, std::variant_alternative_t<3, CVarValue>>));
};

class CVar
{
public:
private:
  CVarValue value;
  mutable std::shared_mutex mtx;

public:
  CVar() = default;

  template<typename T>
  CVar(T&& val)
    : value(std::forward<T>(val))
  {
  }

  template<typename T>
  auto get() const -> std::optional<T>
  {
    std::shared_lock lock(mtx);
    if (auto ptr = std::get_if<T>(&value)) {
      return *ptr;
    }
    return std::nullopt;
  }

  template<typename T>
  auto set(T&& new_value)
  {
    std::unique_lock lock(mtx);
    value = std::forward<T>(new_value);
  }

  template<typename T>
  auto is_type() const
  {
    std::shared_lock lock(mtx);
    return std::holds_alternative<T>(value);
  }
};

}
