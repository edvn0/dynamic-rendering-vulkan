#pragma once

#include "core/forward.hpp"
#include "core/util.hpp"

#include "scene/entity.hpp"

#include <entt/entt.hpp>

class TagRegistry
{
public:
  auto is_unique(std::string_view name) const -> bool
  {
    return !tag_to_entity.contains(name);
  }

  auto register_tag(std::string_view name, entt::entity e) -> bool
  {
    auto key = std::string(name);
    const auto [it, inserted] = tag_to_entity.try_emplace(key, e);
    return inserted;
  }

  auto unregister_tag(std::string_view name) -> void
  {
    tag_to_entity.erase(name);
  }

  auto get_entity(std::string_view name) const -> std::optional<entt::entity>
  {
    if (auto it = tag_to_entity.find(name); it != tag_to_entity.end())
      return it->second;
    return std::nullopt;
  }

  auto generate_unique_name(std::string_view base) const -> std::string
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
  ~Scene() = default;

  auto create_entity(std::string_view) -> Entity;
  auto create_entt_entity() -> entt::entity;
  auto get_registry() -> auto& { return registry; }

  auto on_initialise(const InitialisationParameters&) -> void;
  auto on_interface() -> void;
  auto on_update(double ts) -> void;
  auto on_render(Renderer& renderer) -> void;
  auto on_resize(const EditorCamera&, std::uint32_t w, std::uint32_t h) -> void;
  template<typename... Ts>
  auto view()
  {
    return registry.view<Ts...>();
  }
  template<typename... Ts>
  auto view() const
  {
    return registry.view<Ts...>();
  }
  template<typename... Ts>
  auto each(auto&& func)
  {
    return registry.view<Ts...>().each(std::forward<decltype(func)>(func));
  }
  template<typename... Ts>
  auto each(auto&& func) const
  {
    return registry.view<Ts...>().each(std::forward<decltype(func)>(func));
  }

  auto set_selected_entity(entt::entity entity = entt::null) -> void
  {
    selected_entity = entity;
  }

  auto update_viewport_bounds(const DynamicRendering::ViewportBounds& bounds)
    -> void;

private:
  entt::registry registry;
  std::string scene_name{};
  TagRegistry tag_registry;

  entt::entity selected_entity = entt::null;

  struct SceneCameraComponent
  {
    glm::vec3 position{};
    glm::mat4 view{};
    glm::mat4 projection{};
  };
  Entity scene_camera_entity;

  bool show_components = false;
  bool show_statistics = true;

  glm::vec2 vp_min{};
  glm::vec2 vp_max{};

  auto draw_vector3_slider(const char* label,
                           glm::vec3& value,
                           float v_min,
                           float v_max,
                           const char* format) -> bool;
  auto draw_vector4_slider(const char* label,
                           glm::vec4& value,
                           float v_min,
                           float v_max,
                           const char* format) -> bool;
  auto draw_quaternion_slider(const char* label, glm::quat& quaternion) -> bool;
  auto draw_entity_item(entt::entity entity, std::string_view tag) -> void;

  friend class Entity;
};

#include "entity.inl"