#pragma once

#include <glm/glm.hpp>

struct Frustum
{
  std::array<glm::vec4, 6> planes{};

  static auto from_matrix(const glm::mat4& vp) -> Frustum
  {
    Frustum f;
    f.update(vp);
    return f;
  }

  auto update(const glm::mat4& vp) -> void
  {
    const glm::mat4 m = glm::transpose(vp);

    planes[0] = m[3] + m[0];
    planes[1] = m[3] - m[0];
    planes[2] = m[3] + m[1];
    planes[3] = m[3] - m[1];
    planes[4] = m[3] + m[2];
    planes[5] = m[3] - m[2];

    for (auto& plane : planes)
      plane /= glm::length(glm::vec3(plane));
  }

  [[nodiscard]] auto intersects(const glm::vec3& center, float radius) const
    -> bool
  {
    return std::ranges::none_of(
      planes, [&c = center, &r = radius](const auto& p) {
        return glm::dot(glm::vec3(p), c) + p.w + r < 0.0f;
      });
  }
  [[nodiscard]] auto intersects_aabb(const glm::vec3& min,
                                     const glm::vec3& max) const -> bool
  {
    return std::ranges::none_of(planes, [&min, &max](const auto& p) {
      const auto normal = glm::vec3(p);
      float d = p.w;
      if (normal.x > 0) {
        d += normal.x * min.x;
      } else {
        d += normal.x * max.x;
      }
      if (normal.y > 0) {
        d += normal.y * min.y;
      } else {
        d += normal.y * max.y;
      }
      if (normal.z > 0) {
        d += normal.z * min.z;
      } else {
        d += normal.z * max.z;
      }
      return d < 0.0f;
    });
  }
};