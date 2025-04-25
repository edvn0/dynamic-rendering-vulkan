#include "input.hpp"

#include <utility>

auto
Input::key_pressed(KeyCode code) -> bool
{
  auto state = glfwGetKey(glfw_window, std::to_underlying(code));
  return state == GLFW_PRESS || state == GLFW_REPEAT;
}

auto
Input::mouse_position() -> std::pair<std::uint32_t, std::uint32_t>
{
  double x{};
  double y{};
  glfwGetCursorPos(glfw_window, &x, &y);
  return { static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y) };
}
