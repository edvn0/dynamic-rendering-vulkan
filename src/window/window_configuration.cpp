#include "window/window_configuration.hpp"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <yaml-cpp/yaml.h>

auto
get_default_config_path() -> std::filesystem::path
{
#ifdef _WIN32
  char* appdata = nullptr;
  std::size_t len = 0;
  if (const auto valid = _dupenv_s(&appdata, &len, "APPDATA") == 0;
      valid == 1) {
    std::filesystem::path result = std::filesystem::path(appdata) /
                                   "dynamic_rendering" / "window_config.yaml";
    std::free(appdata);
    return result;
  }
#else
  char* home = std::getenv("HOME");
  if (home)
    return std::filesystem::path(home) / ".dynamic_rendering" /
           "window_config.yaml";
#endif
  return "window_config.yaml";
}
auto
load_window_config(const std::filesystem::path& path) -> WindowConfiguration
{
  WindowConfiguration config;

  if (!std::filesystem::exists(path))
    return config;

  YAML::Node root = YAML::LoadFile(path.string());

  config.size.width = root["width"].as<std::uint32_t>(config.size.width);
  config.size.height = root["height"].as<std::uint32_t>(config.size.height);
  config.fullscreen = root["fullscreen"].as<bool>(config.fullscreen);
  config.resizable = root["resizable"].as<bool>(config.resizable);
  config.decorated = root["decorated"].as<bool>(config.decorated);
  if (root["x"])
    config.x = root["x"].as<int>();
  if (root["y"])
    config.y = root["y"].as<int>();
  if (root["monitor_name"])
    config.monitor_name = root["monitor_name"].as<std::string>();

  return config;
}

auto
save_window_config(const std::filesystem::path& path,
                   Pointers::transparent window) -> void
{
  int x{};
  int y{};
  int w{};
  int h{};

  if (!window) {
    std::cerr << "Window handle is null, cannot save configuration."
              << std::endl;
    return;
  }

  auto* glfw_window = static_cast<GLFWwindow*>(window);
  if (!glfw_window) {
    std::cerr << "Invalid window handle, cannot save configuration."
              << std::endl;
    return;
  }

  glfwGetWindowPos(glfw_window, &x, &y);
  glfwGetWindowSize(glfw_window, &w, &h);

  YAML::Node root;
  root["x"] = x;
  root["y"] = y;
  root["width"] = w;
  root["height"] = h;
  root["fullscreen"] = false;
  root["resizable"] = true;
  root["decorated"] = true;

  GLFWmonitor* monitor = glfwGetWindowMonitor(glfw_window);
  if (!monitor) {
    // fallback: detect monitor based on position
    int monitor_count;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);

    int wx, wy;
    glfwGetWindowPos(glfw_window, &wx, &wy);

    for (int i = 0; i < monitor_count; ++i) {
      int mx, my;
      glfwGetMonitorPos(monitors[i], &mx, &my);

      const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
      if (!mode)
        continue;

      if (wx >= mx && wx < mx + mode->width && wy >= my &&
          wy < my + mode->height) {
        monitor = monitors[i];
        break;
      }
    }
  }

  if (const char* name = monitor ? glfwGetMonitorName(monitor) : nullptr)
    root["monitor_name"] = name;

  std::filesystem::create_directories(path.parent_path());

  std::ofstream fout(path);
  fout << root;
}
