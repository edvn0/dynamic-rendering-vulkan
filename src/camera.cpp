#include "camera.hpp"

#include <algorithm>
#include <array>

#include "input.hpp"

auto
Camera::on_event(Event&) -> bool
{
  return false;
}

auto
Camera::on_update(double delta_time) -> void
{
  glm::vec3 velocity{ 0.0f };

  using enum KeyCode;
  if (Input::key_pressed(W))
    velocity += front;
  if (Input::key_pressed(S))
    velocity -= front;
  if (Input::key_pressed(A))
    velocity -= right;
  if (Input::key_pressed(D))
    velocity += right;
  if (Input::key_pressed(Q))
    velocity -= up;
  if (Input::key_pressed(E))
    velocity += up;

  if (glm::length(velocity) > 0.0f)
    position +=
      glm::normalize(velocity) * speed * static_cast<float>(delta_time);

  if (right_mouse_down && !first_mouse) {
    auto [x, y] = Input::mouse_position();
    auto dx = static_cast<float>(x - last_mouse_x);
    auto dy = static_cast<float>(last_mouse_y - y);
    last_mouse_x = x;
    last_mouse_y = y;

    yaw += dx * sensitivity;
    pitch += dy * sensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    update_vectors();
  }

  set_view(position, position + front, up);
}

auto
Camera::update_vectors() -> void
{
  glm::vec3 f;
  f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  f.y = sin(glm::radians(pitch));
  f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  front = glm::normalize(f);
  right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
  up = glm::normalize(glm::cross(right, front));

  view = glm::lookAt(position, position + front, up);
}