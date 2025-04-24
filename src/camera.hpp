#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
  void set_perspective(float fovy_degrees, float aspect, float znear)
  {
    fov_degrees = fovy_degrees;
    projection = make_infinite_reverse_z_proj(fovy_degrees, aspect, znear);
  }
  void set_view(const glm::vec3& eye,
                const glm::vec3& center,
                const glm::vec3& up)
  {
    view = glm::lookAt(eye, center, up);
  }

  auto get_view() const -> const glm::mat4& { return view; }
  auto get_projection() const -> const glm::mat4& { return projection; }
  auto resize(std::uint32_t width, std::uint32_t height) -> void
  {
    set_perspective(fov_degrees, static_cast<float>(width) / height, 0.1f);
  }

private:
  glm::mat4 view{ 1.0f };
  glm::mat4 projection{ 1.0f };
  float fov_degrees{ 60.0F };

  static auto make_infinite_reverse_z_proj(float fovy,
                                           float aspect,
                                           float znear) -> glm::mat4
  {
    const float f = 1.0f / std::tan(glm::radians(fovy) * 0.5f);
    glm::mat4 result{ 0.0f };
    result[0][0] = f / aspect;
    result[1][1] = -f; // flip Y for Vulkan
    result[2][2] = 0.0f;
    result[2][3] = -1.0f;
    result[3][2] = znear;
    return result;
  }
};
