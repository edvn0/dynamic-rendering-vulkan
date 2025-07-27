#pragma once

#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>

#include "vk-cvar/cvar_value.hpp"
#include "vk-util/util.hpp"

namespace VkCVar {

class CVarManager
{
private:
  VkUtil::string_hash_map<CVar> cvars;
  mutable std::shared_mutex cvar_mutex;

  CVarManager() = default;

public:
  CVarManager(const CVarManager&) = delete;
  CVarManager& operator=(const CVarManager&) = delete;

  static auto the() -> CVarManager&
  {
    static CVarManager instance;
    return instance;
  }

  [[nodiscard]] auto get_readonly_unsafe(const std::string_view name) const
    -> const CVar*
  {
    if (const auto it = cvars.find(name); it != std::cend(cvars)) {
      return &it->second;
    }

    return nullptr;
  }

  [[nodiscard]] auto get(const std::string& name) -> CVar&
  {
    std::shared_lock lock(cvar_mutex);
    auto it = cvars.find(name);
    if (it != cvars.end()) {
      return it->second;
    }
    lock.unlock(); // Release lock before inserting
    std::unique_lock unique_lock(cvar_mutex);
    return cvars[name]; // Insert new CVar
  }

  template<typename T>
  auto set(const std::string& name, T&& value)
  {
    get(name).set(std::forward<T>(value));
  }

  template<typename T>
  auto get(const std::string& name) -> std::optional<T>
  {
    return get(name).get<T>();
  }

  auto reset()
  {
    std::unique_lock unique_lock(cvar_mutex);
    cvars.clear();
  }
};

}
