#pragma once

#include <glm/glm.hpp>

enum class ShadowViewMode : std::uint8_t
{
  LookAtRH,
  LookAtLH,
  Default,
};
enum class ShadowProjectionMode : std::uint8_t
{
  OrthoRH_ZO,
  OrthoRH_NO,
  OrthoLH_ZO,
  OrthoLH_NO,
  Default,
};

struct ColourCorrectionConfig
{
  float exposure{ 1.0f };
  float contrast{ 1.0f };
  float gamma{ 2.2f };
  float saturation{ 1.0f };
  glm::vec3 tint{ 1.0f, 1.0f, 1.0f }; // e.g., vec3(1.0, 0.95, 0.9) for warmth
};

struct LightEnvironment
{
  glm::vec3 light_position{ 400.f, -400.f, 400.f };
  glm::vec4 light_color{ 1.f, 1.f, 1.f, 1.f };
  glm::vec4 ambient_color{ 0.1F, 0.1F, 0.1F, 1.0F };

  float ortho_size{ 50.f };
  float near_plane{ 0.1f };
  float far_plane{ 700.f }; // l2(light_position)
  glm::vec3 target{ 0.F };

  float bloom_strength{ 3.0f };
  ColourCorrectionConfig colour_correction{};

  ShadowProjectionMode projection_mode{ ShadowProjectionMode::Default };
  ShadowViewMode view_mode{ ShadowViewMode::Default };
};

struct CameraEnvironment
{
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 inverse_projection;
  float z_near;
  float z_far;
};