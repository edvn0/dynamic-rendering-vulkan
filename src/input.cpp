#include "input.hpp"

#include <GLFW/glfw3.h>
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

auto
Input::mouse_pressed(MouseCode key) -> bool
{
  auto state = glfwGetMouseButton(glfw_window, std::to_underlying(key));
  return state == GLFW_PRESS;
}
