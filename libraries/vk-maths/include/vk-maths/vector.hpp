#pragma once

#include <glm/glm.hpp>

namespace VkMaths {

inline auto
spherical_to_direction(float azimuth_rad, float elevation_deg) -> glm::vec3
{
  const float elevation_rad = glm::radians(elevation_deg);
  return {
    glm::cos(elevation_rad) * glm::cos(azimuth_rad),
    glm::sin(elevation_rad),
    glm::cos(elevation_rad) * glm::sin(azimuth_rad),
  };
}

}