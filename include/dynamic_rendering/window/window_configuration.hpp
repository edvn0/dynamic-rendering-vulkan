#pragma once

#include "core/extent.hpp"
#include "core/util.hpp"

#include <filesystem>
#include <optional>
#include <string>

struct WindowConfiguration
{
  std::string title{ "Dynamic Rendering" };
  Extent2D size{ 1280, 720 };
  bool resizable{ true };
  bool decorated{ true };
  bool fullscreen{ false };
  std::optional<int> x{};
  std::optional<int> y{};
  std::optional<std::string> monitor_name;
};

auto
get_default_config_path() -> std::filesystem::path;
auto
load_window_config(const std::filesystem::path&) -> WindowConfiguration;

/// @brief Save the window configuration to a file. Allow for a transparent
/// pointer (void*) to be passed in for the window handle. Internally asserts
/// that it is a GLFW window for now.
/// @param  path The path to the file where the configuration will be saved.
/// @param  window The window handle (void*) to be saved in the configuration.
/// @return  void
auto
save_window_config(const std::filesystem::path&, Pointers::transparent) -> void;
