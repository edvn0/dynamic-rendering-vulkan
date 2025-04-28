#include "renderer/camera.hpp"

#include "core/input.hpp"
#include <algorithm>

auto
Camera::on_event(Event&) -> bool
{
  return false;
}

auto
Camera::on_update(double delta_time) -> void
{
  float velocity = speed * static_cast<float>(delta_time);

  using enum KeyCode;
  if (Input::key_pressed(W))
    position += front * velocity;
  if (Input::key_pressed(S))
    position -= front * velocity;
  if (Input::key_pressed(A))
    position -= right * velocity;
  if (Input::key_pressed(D))
    position += right * velocity;
  if (Input::key_pressed(Q))
    position -= world_up * velocity;
  if (Input::key_pressed(E))
    position += world_up * velocity;

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