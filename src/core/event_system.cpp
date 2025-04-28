#include "core/event_system.hpp"

#include <GLFW/glfw3.h>

auto
operator<<(std::ostream& os, KeyCode key) -> std::ostream&
{
  const auto key_name = glfwGetKeyName(std::to_underlying(key), 0);
  if (key_name) {
    os << key_name;
  } else {
    os << "Unknown Key";
  }
  return os;
}