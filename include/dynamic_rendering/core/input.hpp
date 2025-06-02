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
  static auto time() -> float;
  static auto destroy() -> void {}
  static auto initialise(GLFWwindow* window) -> void { glfw_window = window; }

private:
  static inline GLFWwindow* glfw_window{ nullptr };
};

static auto debounce_toggle = [](KeyCode key,
                                 float delay_seconds = 0.2F) -> bool {
  static std::unordered_map<KeyCode, float> last_activation_time;
  const float now = Input::time();

  if (Input::key_pressed(key)) {
    if (float& last = last_activation_time[key]; now - last >= delay_seconds) {
      last = now;
      return true;
    }
  }
  return false;
};
