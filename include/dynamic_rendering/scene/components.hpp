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

} // namespace Component
