#pragma once

#include <glm/glm.hpp>

enum class ShadowViewMode
{
  LookAtRH,
  LookAtLH,
  Default,
};
enum class ShadowProjectionMode
{
  OrthoRH_ZO,
  OrthoRH_NO,
  OrthoLH_ZO,
  OrthoLH_NO,
  Default,
};

struct LightEnvironment
{
  glm::vec3 light_position{ 4.f, -4.f, 4.f };
  glm::vec4 light_color{ 1.f, 1.f, 1.f, 1.f };
  glm::vec4 ambient_color{ 0.1F, 0.1F, 0.1F, 1.0F };

  float ortho_size{ 50.f };
  float near_plane{ 0.1f };
  float far_plane{ 100.f };
  glm::vec3 target{ 0.F };

  float bloom_strength{ 3.0f };

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