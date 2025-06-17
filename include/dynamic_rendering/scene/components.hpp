#pragma once

#include "core/forward.hpp"

#include "assets/handle.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace Component {

struct Transform
{
  glm::vec3 position{ 0.f };
  glm::quat rotation{ 1.f, 0.f, 0.f, 0.f };
  glm::vec3 scale{ 1.f };

  [[nodiscard]] auto compute() const -> glm::mat4
  {
    return glm::translate(glm::mat4(1.f), position) * glm::mat4_cast(rotation) *
           glm::scale(glm::mat4(1.f), scale);
  }
};

struct Tag
{
  std::string name;
};

struct Hierarchy
{
  entt::entity parent{ entt::null };
  std::vector<entt::entity> children;
};

struct Mesh
{
  Assets::Handle<StaticMesh> mesh;
  bool casts_shadows{ true };
  bool draw_aabb{ false };

  Mesh() = default;
  explicit Mesh(Assets::Handle<StaticMesh>);
  explicit Mesh(std::string_view path);
};

struct Material
{
  Assets::Handle<::Material> material;

  Material() = default;
  explicit Material(Assets::Handle<::Material> m)
    : material(m)
  {
  }
  explicit Material(std::string_view path);
};

struct Camera
{
  float fov{ 60.0f };
  float aspect{ 16.0f / 9.0f };
  float znear{ 0.1f };
  float zfar{ 1000.0f };

  glm::mat4 projection{ 1.0f };
  glm::mat4 inverse_projection{ 1.0f };

  bool dirty{ true };
  [[nodiscard]] auto clean() const { return !dirty; }
  auto on_resize(const float new_fov, const float new_aspect) -> void
  {
    fov = new_fov;
    aspect = new_aspect;
    dirty = true;
  }
  auto on_resize(const float new_aspect) -> void { on_resize(fov, new_aspect); }
};

struct FlyController
{
  float move_speed{ 5.0f };
  float rotation_speed{ 0.1f };
  bool active{ true };
};

} // namespace Component
