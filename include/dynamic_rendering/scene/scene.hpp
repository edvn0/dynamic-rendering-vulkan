#pragma once

#include "core/util.hpp"
#include "scene/entity.hpp"

#include <entt/entt.hpp>

class TagRegistry
{
public:
  auto is_unique(std::string_view name) const -> bool
  {
    return tag_to_entity.find(name) == tag_to_entity.end();
  }

  auto register_tag(std::string_view name, entt::entity e) -> bool
  {
    const auto [it, inserted] = tag_to_entity.try_emplace(std::string(name), e);
    return inserted;
  }

  auto unregister_tag(std::string_view name) -> void
  {
    tag_to_entity.erase(std::string(name));
  }

  auto get_entity(std::string_view name) const -> std::optional<entt::entity>
  {
    if (auto it = tag_to_entity.find(name); it != tag_to_entity.end())
      return it->second;
    return std::nullopt;
  }

  auto generate_unique_name(std::string_view base) -> std::string
  {
    auto counter = 1;
    std::string candidate;
    do {
      candidate = std::format("{}_{}", base, counter++);
    } while (!is_unique(candidate));
    return candidate;
  }

private:
  string_hash_map<entt::entity> tag_to_entity;
};

class Scene
{
public:
  explicit Scene(std::string_view);

  auto create_entity(std::string_view) -> Entity;
  auto create_entt_entity() -> entt::entity;
  auto get_registry() -> auto& { return registry; }

private:
  entt::registry registry;
  std::string scene_name{};
  TagRegistry tag_registry;

  friend class Entity;
};

#include "entity.inl"