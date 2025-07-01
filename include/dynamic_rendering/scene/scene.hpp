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

  auto register_tag(const std::string_view name, entt::entity e) -> bool
  {
    const auto key = std::string(name);
    const auto [it, inserted] =
      tag_to_entity.try_emplace(key, TagTuple{ e, allocate_unique_id() });
    return inserted;
  }

  auto unregister_tag(const std::string_view name) -> void
  {
    if (const auto it = tag_to_entity.find(name); it != tag_to_entity.end()) {
      deallocate_unique_id(it->second.unique_id);
      tag_to_entity.erase(it);
    }
  }

  auto get_entity(const std::string_view name) const
    -> std::optional<entt::entity>
  {
    if (const auto it = tag_to_entity.find(name); it != tag_to_entity.end())
      return it->second.entity;
    return std::nullopt;
  }

  auto get_unique_id(const std::string_view name) const
    -> std::optional<std::uint64_t>
  {
    if (const auto it = tag_to_entity.find(name); it != tag_to_entity.end())
      return it->second.unique_id;
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
  struct TagTuple
  {
    entt::entity entity;
    std::uint32_t unique_id;
  };

  auto allocate_unique_id() -> std::uint32_t
  {
    if (!free_ids.empty()) {
      const auto id = free_ids.back();
      free_ids.pop_back();
      return id;
    }
    return next_id++;
  }

  auto deallocate_unique_id(const std::uint32_t id) -> void
  {
    if (id + 1 == next_id) {
      --next_id;

      while (!free_ids.empty() && free_ids.back() + 1 == next_id) {
        free_ids.pop_back();
        --next_id;
      }
    } else {
      free_ids.insert(
        std::ranges::upper_bound(free_ids, id, std::greater<std::uint32_t>()),
        id);
    }
  }

  string_hash_map<TagTuple> tag_to_entity;
  std::uint32_t next_id{ 1 }; // Start at 1, reserve 0 for "invalid"
  std::vector<std::uint32_t> free_ids;
};

class Scene
{
public:
  explicit Scene(std::string_view);
  ~Scene() = default;

  auto create_entity(std::string_view) -> Entity;
  auto create_entt_entity() -> entt::entity;
  auto get_registry() -> auto& { return registry; }

  auto on_interface() -> void;
  auto on_update(double ts) -> void;
  auto on_render(Renderer& renderer) -> void;
  auto on_event(Event&) -> bool;
  auto on_resize(const EditorCamera&) -> void;
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
  auto get_selected_entity() const -> entt::entity { return selected_entity; }

  auto update_viewport_bounds(const DynamicRendering::ViewportBounds& bounds)
    -> void;
  [[nodiscard]] auto selected_is_valid() const -> bool;
  auto delete_entity(entt::entity) -> void;
  auto delete_entity() -> void { delete_entity(selected_entity); }

private:
  entt::registry registry;
  std::string scene_name{};
  TagRegistry tag_registry;

  entt::entity selected_entity = entt::null;

  Entity scene_camera_entity;

  bool show_components = true;
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
  auto draw_entity_hierarchy(entt::entity entity, const std::string&) -> bool;
  auto has_matching_child(entt::entity entity, const std::string&) -> bool;
  auto update_fly_controllers(double) -> void;

  friend class Entity;
};

#include "entity.inl"