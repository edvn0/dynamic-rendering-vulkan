#pragma once

#include "core/event_system.hpp"
#include "core/input.hpp"
#include "renderer/camera.hpp"

#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

enum class CameraMode
{
  NONE,
  FLYCAM,
  ARCBALL
};

class EditorCamera : public Camera
{
public:
  EditorCamera(float fovy_degrees, float aspect, float znear, float zfar)
  {
    set_perspective_float_far(fovy_degrees, aspect, znear, zfar);
    init();
  }

  auto init() -> void
  {
    position = glm::vec3(0.F, 5.0f, -10.0f);
    focal_point = glm::vec3(0.0f, 0.0f, 0.0f);
    distance = glm::distance(position, focal_point);

    position = calculate_position();
    const auto orientation = get_orientation();
    direction = glm::eulerAngles(orientation) * (180.0f / glm::pi<float>());

    const auto transform =
      glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(orientation);
    view = glm::inverse(transform);

    update_camera_vectors();
  }

  auto on_update(double delta_time) -> void override
  {
    const float delta_time_ms = static_cast<float>(delta_time * 1000.0f);
    auto [mouse_x, mouse_y] = Input::mouse_position();
    const glm::vec2 mouse_pos(static_cast<float>(mouse_x),
                              static_cast<float>(mouse_y));
    const glm::vec2 delta = (mouse_pos - initial_mouse_position) * 0.002f;

    if (!is_active()) {
      return;
    }

    if (Input::mouse_pressed(MouseCode::MouseButtonRight) &&
        !Input::key_pressed(KeyCode::LeftAlt)) {
      camera_mode = CameraMode::FLYCAM;

      const float yaw_sign = get_up_direction().y < 0.0f ? -1.0f : 1.0f;
      const float current_speed = get_camera_speed() * delta_time_ms;

      // Calculate a stable right vector for movement
      glm::vec3 world_up_vector(0.0f, 1.0f, 0.0f); // +Y is up in Vulkan
      glm::vec3 movement_right =
        glm::normalize(glm::cross(world_up_vector, direction));

      // Handle keyboard movement
      if (Input::key_pressed(KeyCode::Q))
        position_delta -= current_speed * world_up_vector;
      if (Input::key_pressed(KeyCode::E))
        position_delta += current_speed * world_up_vector;
      if (Input::key_pressed(KeyCode::S))
        position_delta -= current_speed * direction;
      if (Input::key_pressed(KeyCode::W))
        position_delta += current_speed * direction;
      if (Input::key_pressed(KeyCode::A))
        position_delta -= current_speed * movement_right;
      if (Input::key_pressed(KeyCode::D))
        position_delta += current_speed * movement_right;

      // Mouse rotation logic
      const float max_rate = 0.12f;
      yaw_delta +=
        std::clamp(yaw_sign * delta.x * rotation_speed(), -max_rate, max_rate);
      pitch_delta +=
        std::clamp(delta.y * rotation_speed(), -max_rate, max_rate);

      // First apply yaw (rotation around world up axis)
      glm::quat yaw_rotation = glm::angleAxis(-yaw_delta, world_up_vector);
      direction = yaw_rotation * direction;

      // Recalculate right vector after yaw rotation
      right_direction = glm::normalize(glm::cross(direction, world_up_vector));

      // Then apply pitch (rotation around right axis)
      glm::quat pitch_rotation = glm::angleAxis(-pitch_delta, right_direction);
      direction = pitch_rotation * direction;

      // Normalize direction
      direction = glm::normalize(direction);

      const float distance_value = glm::distance(focal_point, position);
      focal_point = position + get_forward_direction() * distance_value;
      distance = distance_value;
    } else if (Input::key_pressed(KeyCode::LeftAlt)) {
      camera_mode = CameraMode::ARCBALL;

      if (Input::mouse_pressed(MouseCode::MouseButtonMiddle)) {
        mouse_pan(delta);
      } else if (Input::mouse_pressed(MouseCode::MouseButtonLeft)) {
        mouse_rotate(delta);
      } else if (Input::mouse_pressed(MouseCode::MouseButtonRight)) {
        mouse_zoom(delta.x + delta.y);
      }
    }

    initial_mouse_position = mouse_pos;
    position += position_delta;
    yaw += yaw_delta;
    pitch += pitch_delta;

    if (camera_mode == CameraMode::ARCBALL) {
      position = calculate_position();
    }

    update_camera_view();
  }

  auto on_event(Event& event) -> bool override
  {
    EventDispatcher dispatch(event);
    dispatch.dispatch<MouseScrolledEvent>([this](auto& scroll_event) {
      const auto as_float = static_cast<float>(scroll_event.y_offset);
      if (Input::mouse_pressed(MouseCode::MouseButtonRight)) {
        normal_speed += as_float * 0.3f * normal_speed;
        normal_speed = glm::clamp(normal_speed, MIN_SPEED, MAX_SPEED);
      } else {
        mouse_zoom(as_float * 0.1f);
        update_camera_view();
      }

      return true;
    });

    return false;
  }

  auto focus(const glm::vec3& focus_point) -> void
  {
    focal_point = focus_point;
    camera_mode = CameraMode::FLYCAM;

    if (distance > min_focus_distance) {
      distance -= distance - min_focus_distance;
      position = focal_point - get_forward_direction() * distance;
    }

    position = focal_point - get_forward_direction() * distance;
    update_camera_view();
  }

  auto resize(std::uint32_t width, std::uint32_t height) -> void override
  {
    Camera::resize(width, height);
    viewport_width = width;
    viewport_height = height;
  }

  auto set_active(bool act) -> void { active = act; }
  auto is_active() const -> bool { return active; }
  auto get_camera_mode() const -> CameraMode { return camera_mode; }
  auto get_distance() const -> float { return distance; }
  auto set_distance(float new_distance) -> void { distance = new_distance; }
  auto get_focal_point() const -> const glm::vec3& { return focal_point; }
  auto get_position() const -> const glm::vec3& { return position; }
  auto get_pitch() const -> float { return pitch; }
  auto get_yaw() const -> float { return yaw; }

private:
  auto update_camera_view() -> void
  {
    const float yaw_sign = get_up_direction().y < 0.0f ? -1.0f : 1.0f;

    const float cos_angle =
      glm::dot(get_forward_direction(), get_up_direction());
    if (cos_angle * yaw_sign > 0.99f) {
      pitch_delta = 0.0f;
    }

    const glm::vec3 look_at = position + get_forward_direction();
    direction = glm::normalize(look_at - position);
    distance = glm::distance(position, focal_point);
    view = glm::lookAt(position, look_at, glm::vec3(0.0f, yaw_sign, 0.0f));

    yaw_delta *= 0.6f;
    pitch_delta *= 0.6f;
    position_delta *= 0.8f;
  }

  auto pan_speed() const -> std::pair<float, float>
  {
    const float x =
      std::min(static_cast<float>(viewport_width) / 1000.0f, 2.4f);
    const float x_factor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

    const float y =
      std::min(static_cast<float>(viewport_height) / 1000.0f, 2.4f);
    const float y_factor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

    return { x_factor, y_factor };
  }

  auto rotation_speed() const -> float { return 0.3f; }

  auto zoom_speed() const -> float
  {
    float z_speed = distance * 0.2f;
    z_speed = std::max(z_speed, 0.0f);
    z_speed = z_speed * z_speed;
    return std::min(z_speed, 50.0f);
  }

  auto get_camera_speed() const -> float
  {
    float cam_speed = normal_speed;
    if (Input::key_pressed(KeyCode::LeftControl))
      cam_speed /= 2.0f - glm::log(normal_speed);
    if (Input::key_pressed(KeyCode::LeftShift))
      cam_speed *= 2.0f - glm::log(normal_speed);

    return glm::clamp(cam_speed, MIN_SPEED, MAX_SPEED);
  }

  auto mouse_pan(const glm::vec2& delta) -> void
  {
    auto [x_speed, y_speed] = pan_speed();
    focal_point -= get_right_direction() * delta.x * x_speed * distance;
    focal_point += get_up_direction() * delta.y * y_speed * distance;
  }

  auto mouse_rotate(const glm::vec2& delta) -> void
  {
    const float yaw_sign = get_up_direction().y < 0.0f ? -1.0f : 1.0f;
    yaw_delta += yaw_sign * delta.x * rotation_speed();
    pitch_delta += delta.y * rotation_speed();
  }

  auto mouse_zoom(float delta) -> void
  {
    distance -= delta * zoom_speed();
    const glm::vec3 forward_dir = get_forward_direction();
    position = focal_point - forward_dir * distance;

    if (distance < 1.0f) {
      focal_point += forward_dir * distance;
      distance = 1.0f;
    }

    position_delta += delta * zoom_speed() * forward_dir;
  }

  auto get_orientation() const -> glm::quat
  {
    return glm::quat(glm::vec3(-pitch - pitch_delta, -yaw - yaw_delta, 0.0f));
  }

  auto calculate_position() const -> glm::vec3
  {
    return focal_point - get_forward_direction() * distance + position_delta;
  }

  auto get_up_direction() const -> glm::vec3
  {
    return glm::rotate(get_orientation(), glm::vec3(0.0f, 1.0f, 0.0f));
  }

  auto get_right_direction() const -> glm::vec3
  {
    return glm::rotate(get_orientation(), glm::vec3(1.0f, 0.0f, 0.0f));
  }

  auto get_forward_direction() const -> glm::vec3
  {
    return glm::rotate(get_orientation(), glm::vec3(0.0f, 0.0f, -1.0f));
  }

  CameraMode camera_mode = CameraMode::ARCBALL;
  bool active = true;

  glm::vec3 focal_point = glm::vec3(0.0f);
  glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 right_direction = glm::vec3(1.0f, 0.0f, 0.0f);
  float distance = 10.0f;

  glm::vec3 position_delta = glm::vec3(0.0f);
  float yaw_delta = 0.0f;
  float pitch_delta = 0.0f;

  glm::vec2 initial_mouse_position = glm::vec2(0.0f);

  float normal_speed = 0.002f;
  float min_focus_distance = 100.0f;
  uint32_t viewport_width = 1280;
  uint32_t viewport_height = 720;

  static constexpr float MIN_SPEED = 0.0005f;
  static constexpr float MAX_SPEED = 2.0f;

  using Camera::front;
  using Camera::pitch;
  using Camera::position;
  using Camera::right;
  using Camera::up;
  using Camera::update_camera_vectors;
  using Camera::view;
  using Camera::world_up;
  using Camera::yaw;
};