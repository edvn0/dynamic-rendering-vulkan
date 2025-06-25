#pragma once

#include <glm/vec3.hpp>

/// @brief GPU-side point light data structure (matches GLSL layout)
struct GPUPointLight
{
  alignas(16) glm::vec3 position;
  float radius;
  alignas(16) glm::vec3 color;
  float intensity;

  // float attenuation_constant = 1.0f;
  // float attenuation_linear = 0.09f;
  // float attenuation_quadratic = 0.032f;
  // std::uint32_t shadow_index = UINT32_MAX;
};
static_assert(sizeof(GPUPointLight) == 32,
              "GPUPointLight must be 32 bytes for optimal alignment.");
