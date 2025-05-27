#pragma once

#include <entt/entt.hpp>
#include <string_view>

class Scene;
class ReadonlyEntity;

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

  [[nodiscard]] auto raw() const -> entt::entity { return handle; }

private:
  entt::entity handle{ entt::null };
  Scene* scene{ nullptr };

  Entity(entt::entity, Scene*);

  friend class Scene;
  friend class ReadonlyEntity;
};

class ReadonlyEntity
{
public:
  ReadonlyEntity(entt::entity e, Scene* s)
    : entity(e, s)
  {
  }

  template<typename T>
  [[nodiscard]] auto try_get() const -> const T*;

private:
  Entity entity{};

  friend class Entity;
};