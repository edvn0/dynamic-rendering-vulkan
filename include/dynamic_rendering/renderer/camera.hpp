#pragma once

#include "core/event_system.hpp"
#include "core/input.hpp"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <variant>

namespace camera_constants {
constexpr glm::vec3 WORLD_FORWARD = glm::vec3(0.0f, 0.0f, -1.0f);
constexpr glm::vec3 WORLD_RIGHT = glm::vec3(1.0f, 0.0f, 0.0f);
constexpr glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
}

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

  virtual auto on_event(Event&) -> bool { return false; }

  virtual auto on_update(double delta_time) -> void
  {
    float dt = static_cast<float>(delta_time);
    glm::vec3 move_dir{ 0.0f };

    using enum KeyCode;
    if (Input::key_pressed(W))
      move_dir += front;
    if (Input::key_pressed(S))
      move_dir -= front;
    if (Input::key_pressed(D))
      move_dir += right;
    if (Input::key_pressed(A))
      move_dir -= right;
    if (Input::key_pressed(Q))
      move_dir += world_up;
    if (Input::key_pressed(E))
      move_dir -= world_up;

    if (glm::length(move_dir) > 0.001f)
      move_dir = glm::normalize(move_dir);

    acceleration = move_dir * speed;
    velocity += acceleration * dt;
    velocity *= std::exp(-damping * dt);

    position += velocity * dt;

    if (Input::mouse_pressed(MouseCode::MouseButtonRight)) {
      auto [x, y] = Input::mouse_position();

      if (first_mouse) {
        last_mouse_x = x;
        last_mouse_y = y;
        first_mouse = false;
      }

      float x_offset = static_cast<float>(x - last_mouse_x);
      float y_offset = static_cast<float>(last_mouse_y - y);

      last_mouse_x = x;
      last_mouse_y = y;

      x_offset *= sensitivity;
      y_offset *= sensitivity;

      yaw += x_offset;
      pitch += y_offset;
      pitch = std::clamp(pitch, -89.0f, 89.0f);

      update_camera_vectors();
    } else {
      first_mouse = true;
    }

    update_view_matrix();
  }

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

  auto set_perspective(float fovy_degrees, float aspect, float znear) -> void
  {
    projection_config = InfiniteProjection{ fovy_degrees, aspect, znear };
    update_projection_matrix();
  }

  auto set_perspective_float_far(float fovy_degrees,
                                 float aspect,
                                 float znear,
                                 float zfar) -> void
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
  auto get_inverse_projection() const -> const glm::mat4&
  {
    return inverse_projection;
  }
  auto compute_view_projection() const -> glm::mat4
  {
    return projection * view;
  }
  auto compute_inverse_view_projection() const -> glm::mat4
  {
    return inverse_projection * view;
  }

  auto get_position() const -> const glm::vec3& { return position; }
  auto get_front() const -> const glm::vec3& { return front; }
  auto get_right() const -> const glm::vec3& { return right; }
  auto get_up() const -> const glm::vec3& { return up; }
  auto get_basis_matrix() const -> glm::mat3
  {
    return glm::mat3(right, up, -front);
  }

  auto get_projection_config() const -> const ProjectionConfig&
  {
    return projection_config;
  }

protected:
  glm::mat4 view{ 1.0f };
  glm::mat4 projection{ 1.0f };
  glm::mat4 inverse_projection{ 1.0f };

  ProjectionConfig projection_config{
    InfiniteProjection{ 60.0f, 16.0f / 9.0f, 0.1f }
  };

  glm::vec3 position{ 0.0f, 3.0f, -2.0f };
  glm::vec3 front{ camera_constants::WORLD_FORWARD };
  glm::vec3 right{ camera_constants::WORLD_RIGHT };
  glm::vec3 up{ camera_constants::WORLD_UP };
  glm::vec3 world_up{ camera_constants::WORLD_UP };

  glm::vec3 velocity{ 0.0f };
  glm::vec3 acceleration{ 0.0f };

  float yaw{ -90.0f };
  float pitch{ 0.0f };
  float speed{ 5.0f };
  float sensitivity{ 0.1f };
  float damping{ 10.0f };

  double last_mouse_x{ 0.0 };
  double last_mouse_y{ 0.0 };
  bool first_mouse{ true };

  static auto make_infinite_reverse_z_proj(float fovy,
                                           float aspect,
                                           float znear) -> glm::mat4
  {
    float f = 1.0f / std::tan(glm::radians(fovy) * 0.5f);
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
          inverse_projection = make_float_far_proj(
            config.fov, config.aspect, config.zfar, config.znear);
        }
      },
      projection_config);
  }

  auto update_camera_vectors() -> void
  {
    front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    front.y = std::sin(glm::radians(pitch));
    front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    front = glm::normalize(front);
    right = glm::normalize(glm::cross(front, world_up));
    up = glm::normalize(glm::cross(right, front));
  }

  auto update_view_matrix() -> void
  {
    view = glm::lookAt(position, position + front, up);
  }
};