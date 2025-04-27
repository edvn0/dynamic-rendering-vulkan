#pragma once
#include "event_system.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <variant>

class Camera
{
public:
  struct InfiniteProjection
  {
    float fov;
    float aspect;
    float znear;
  };
  struct FloatFarProjection
  {
    float fov;
    float aspect;
    float znear;
    float zfar;
  };

  using ProjectionConfig = std::variant<InfiniteProjection, FloatFarProjection>;

  virtual ~Camera() = default;

  virtual auto on_event(Event& event) -> bool;
  virtual auto on_update(double delta_time) -> void;

  auto get_projection_config() const { return projection_config; }

  auto set_perspective(float fovy_degrees, float aspect, float znear)
  {
    projection_config = InfiniteProjection{ fovy_degrees, aspect, znear };
    update_projection_matrix();
  }

  auto set_perspective_float_far(float fovy_degrees,
                                 float aspect,
                                 float znear,
                                 float zfar)
  {
    projection_config = FloatFarProjection{ fovy_degrees, aspect, znear, zfar };
    update_projection_matrix();
  }

  void set_view(const glm::vec3& eye,
                const glm::vec3& center,
                const glm::vec3& up_dir)
  {
    position = eye;
    front = glm::normalize(center - eye);
    right = glm::normalize(glm::cross(front, up_dir));
    up = glm::normalize(glm::cross(right, front));
    update_view_matrix();
  }

  auto get_view() const -> const glm::mat4& { return view; }
  auto get_projection() const -> const glm::mat4& { return projection; }

  virtual auto resize(std::uint32_t width, std::uint32_t height) -> void
  {
    if (std::holds_alternative<InfiniteProjection>(projection_config)) {
      auto& inf = std::get<InfiniteProjection>(projection_config);
      set_perspective(inf.fov, static_cast<float>(width) / height, inf.znear);
    } else if (std::holds_alternative<FloatFarProjection>(projection_config)) {
      auto& ff = std::get<FloatFarProjection>(projection_config);
      set_perspective_float_far(
        ff.fov, static_cast<float>(width) / height, ff.znear, ff.zfar);
    }
  }

protected:
  glm::mat4 view{ 1.0f };
  glm::mat4 projection{ 1.0f };
  float fov_degrees{ 60.0f };
  ProjectionConfig projection_config{
    InfiniteProjection{ 60.0f, 16.0f / 9.0f, 0.1f }
  };

  glm::vec3 position{ 0.0f, 3.0f, -2.0f };
  glm::vec3 front{ 0.0f, 0.0f, -1.0f };
  glm::vec3 up{ 0.0f, 1.0f, 0.0f };
  glm::vec3 right{ 1.0f, 0.0f, 0.0f };
  glm::vec3 world_up{ 0.0f, 1.0f, 0.0f };

  float yaw{ -90.0f };
  float pitch{ 0.0f };

  float speed{ 5.0f };
  float sensitivity{ 0.1f };

  double last_mouse_x{ 0.0 };
  double last_mouse_y{ 0.0 };
  bool first_mouse{ true };

  static auto make_infinite_reverse_z_proj(float fovy,
                                           float aspect,
                                           float znear) -> glm::mat4
  {
    const float f = 1.0f / std::tan(glm::radians(fovy) * 0.5f);
    glm::mat4 result{ 0.0f };
    result[0][0] = f / aspect;
    result[1][1] = -f;
    result[2][2] = 0.0f;
    result[2][3] = -1.0f;
    result[3][2] = znear;
    return result;
  }

  static auto make_float_far_proj(float fovy,
                                  float aspect,
                                  float znear,
                                  float zfar) -> glm::mat4
  {
    return glm::perspective(glm::radians(fovy), aspect, znear, zfar);
  }

  auto update_projection_matrix() -> void
  {
    std::visit(
      [this](auto&& config) {
        using T = std::decay_t<decltype(config)>;
        if constexpr (std::is_same_v<T, InfiniteProjection>) {
          projection = make_infinite_reverse_z_proj(
            config.fov, config.aspect, config.znear);
        } else if constexpr (std::is_same_v<T, FloatFarProjection>) {
          projection = make_float_far_proj(
            config.fov, config.aspect, config.znear, config.zfar);
        }
      },
      projection_config);
  }

  auto update_camera_vectors() -> void
  {
    glm::vec3 new_front;
    new_front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    new_front.y = sin(glm::radians(pitch));
    new_front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(new_front);
    right = glm::normalize(glm::cross(front, world_up));
    up = glm::normalize(glm::cross(right, front));
  }

  auto update_view_matrix() -> void
  {
    view = glm::lookAt(position, position + front, up);
  }
};