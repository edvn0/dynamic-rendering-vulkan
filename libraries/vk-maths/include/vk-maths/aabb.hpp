#pragma once

#include <glm/glm.hpp>

namespace VkMaths {

class AABB
{
public:
  AABB();

  explicit AABB(const glm::vec3&);
  explicit AABB(const glm::vec3& min, const glm::vec3& max);

  [[nodiscard]] auto min() const noexcept -> const glm::vec3&;
  [[nodiscard]] auto max() const noexcept -> const glm::vec3&;

  auto grow(const glm::vec3& point) noexcept -> void;
  auto grow(const AABB& other) noexcept -> void;

  [[nodiscard]] auto is_valid() const noexcept -> bool;
  [[nodiscard]] auto is_empty() const noexcept -> bool;
  [[nodiscard]] auto center() const noexcept -> glm::vec3;
  [[nodiscard]] auto extent() const noexcept -> glm::vec3;

  [[nodiscard]] auto transformed(const glm::mat4& m) const noexcept -> AABB;
  [[nodiscard]] auto uniform_scale(
    const glm::vec3& scaling_factors) const noexcept -> AABB;

private:
  glm::vec3 minimum;
  glm::vec3 maximum;
};

}
