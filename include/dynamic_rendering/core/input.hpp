#pragma once

#include "event_system.hpp"

extern "C"
{
  struct GLFWwindow;
}

class Input
{
public:
  static auto key_pressed(KeyCode key) -> bool;
  static auto mouse_position() -> std::pair<std::uint32_t, std::uint32_t>;
  static auto mouse_pressed(MouseCode key) -> bool;
  static auto destroy() -> void {}
  static auto initialise(GLFWwindow* window) -> void { glfw_window = window; }

private:
  static inline GLFWwindow* glfw_window{ nullptr };
};