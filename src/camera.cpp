#include "camera.hpp"

#include <algorithm>
#include <array>

auto
Camera::on_event(Event& event) -> bool
{
  EventDispatcher dispatcher(event);
  dispatcher.dispatch<KeyPressedEvent>([&](KeyPressedEvent& e) {
    switch (e.key) {
      using enum KeyCode;
      case W:
        movement[0] = true;
        break;
      case S:
        movement[1] = true;
        break;
      case A:
        movement[2] = true;
        break;
      case D:
        movement[3] = true;
        break;
      case Q:
        movement[4] = true;
        break;
      case E:
        movement[5] = true;
        break;
      default:
        break;
    }
    return false;
  });
  dispatcher.dispatch<KeyReleasedEvent>([&](KeyReleasedEvent& e) {
    switch (e.key) {
      using enum KeyCode;
      case W:
        movement[0] = false;
        break;
      case S:
        movement[1] = false;
        break;
      case A:
        movement[2] = false;
        break;
      case D:
        movement[3] = false;
        break;
      case Q:
        movement[4] = false;
        break;
      case E:
        movement[5] = false;
        break;
      default:
        break;
    }
    return false;
  });
  dispatcher.dispatch<MouseButtonPressedEvent>([&](auto& e) {
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT)
      right_mouse_down = true;
    return false;
  });
  dispatcher.dispatch<MouseButtonReleasedEvent>([&](auto& e) {
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT)
      right_mouse_down = false;
    return false;
  });
  dispatcher.dispatch<MouseMovedEvent>([&](MouseMovedEvent& e) {
    if (!right_mouse_down)
      return false;
    if (first_mouse) {
      last_mouse_x = e.x;
      last_mouse_y = e.y;
      first_mouse = false;
      return false;
    }
    auto dx = static_cast<float>(e.x - last_mouse_x);
    auto dy = static_cast<float>(last_mouse_y - e.y); // flip Y
    last_mouse_x = e.x;
    last_mouse_y = e.y;

    yaw += dx * sensitivity;
    pitch += dy * sensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    update_vectors();
    return true;
  });
  return false;
}

auto
Camera::on_update(double delta_time) -> void
{
  glm::vec3 velocity{ 0.0f };
  if (movement[0])
    velocity += front;
  if (movement[1])
    velocity -= front;
  if (movement[2])
    velocity -= right;
  if (movement[3])
    velocity += right;
  if (movement[4])
    velocity -= up;
  if (movement[5])
    velocity += up;

  if (glm::length(velocity) > 0.0f)
    position +=
      glm::normalize(velocity) * speed * static_cast<float>(delta_time);

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