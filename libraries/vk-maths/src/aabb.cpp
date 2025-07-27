#include "vk-maths/aabb.hpp"

#include <array>

namespace VkMaths {

AABB::AABB()
  : minimum{ std::numeric_limits<float>::max() }
  , maximum{ std::numeric_limits<float>::lowest() }
{
}

AABB::AABB(const glm::vec3& initial_point)
  : minimum{ initial_point }
  , maximum{ initial_point }
{
}

AABB::AABB(const glm::vec3& min, const glm::vec3& max)
  : minimum(min)
  , maximum(max)
{
}

auto
AABB::min() const noexcept -> const glm::vec3&
{
  return minimum;
}
[[nodiscard]] auto
AABB::max() const noexcept -> const glm::vec3&
{
  return maximum;
}

auto
AABB::grow(const glm::vec3& point) noexcept -> void
{
  if (point.x < minimum.x)
    minimum.x = point.x;
  if (point.y < minimum.y)
    minimum.y = point.y;
  if (point.z < minimum.z)
    minimum.z = point.z;

  if (point.x > maximum.x)
    maximum.x = point.x;
  if (point.y > maximum.y)
    maximum.y = point.y;
  if (point.z > maximum.z)
    maximum.z = point.z;
}

auto
AABB::grow(const AABB& other) noexcept -> void
{
  grow(other.minimum);
  grow(other.maximum);
}

[[nodiscard]] auto
AABB::is_valid() const noexcept -> bool
{
  return minimum.x <= maximum.x && minimum.y <= maximum.y &&
         minimum.z <= maximum.z;
}

[[nodiscard]] auto
AABB::is_empty() const noexcept -> bool
{
  return minimum == maximum;
}

[[nodiscard]] auto
AABB::center() const noexcept -> glm::vec3
{
  return 0.5f * (minimum + maximum);
}

[[nodiscard]] auto
AABB::extent() const noexcept -> glm::vec3
{
  return maximum - minimum;
}

[[nodiscard]] auto
AABB::transformed(const glm::mat4& m) const noexcept -> AABB
{
  const std::array<glm::vec3, 8> corners = {
    glm::vec3{ minimum.x, minimum.y, minimum.z },
    { maximum.x, minimum.y, minimum.z },
    { minimum.x, maximum.y, minimum.z },
    { maximum.x, maximum.y, minimum.z },
    { minimum.x, minimum.y, maximum.z },
    { maximum.x, minimum.y, maximum.z },
    { minimum.x, maximum.y, maximum.z },
    { maximum.x, maximum.y, maximum.z }
  };

  AABB result;
  for (const auto& c : corners)
    result.grow(glm::vec3(m * glm::vec4(c, 1.0f)));
  return result;
}

auto
AABB::uniform_scale(const glm::vec3& scaling_factors) const noexcept -> AABB
{
  return AABB(minimum * scaling_factors, maximum * scaling_factors);
}

}