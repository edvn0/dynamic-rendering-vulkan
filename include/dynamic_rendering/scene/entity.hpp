#pragma once

#include <entt/entt.hpp>
#include <string_view>

class Scene;

class Entity
{
public:
  Entity() = default;

  template<typename T, typename... Args>
  auto add_component(Args&&... args) -> T&;

  template<typename T>
  auto get_component() -> T&;

  template<typename T>
  auto try_get() -> T*;

  template<typename T>
  auto has_component() const -> bool;

  auto raw() const -> entt::entity { return handle; }

private:
  entt::entity handle{ entt::null };
  Scene* scene{ nullptr };

  Entity(entt::entity, Scene*);

  friend class Scene;
};