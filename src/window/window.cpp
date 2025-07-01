#include "window/window.hpp"

#include "core/input.hpp"
#include "core/instance.hpp"

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <bit>
#include <cassert>

struct Window::WindowData
{
  bool framebuffer_resized{ false };
  std::function<void(Event&)> event_callback = [](Event&) {};
};

auto
Window::create(const WindowConfiguration& config, std::filesystem::path path)
  -> std::unique_ptr<Window>
{
  auto window = std::unique_ptr<Window>(new Window(config));
  window->config_path = std::move(path);
  return window;
}

auto
Window::hookup_events() -> void
{
  glfwSetFramebufferSizeCallback(
    glfw_window, +[](GLFWwindow* w, int width, int height) {
      auto& d = *static_cast<Window::WindowData*>(glfwGetWindowUserPointer(w));
      WindowResizeEvent ev{ width, height };
      d.event_callback(ev);
      d.framebuffer_resized = true;
    });
  glfwSetKeyCallback(
    glfw_window, [](GLFWwindow* w, int key, int, int action, int mods) {
      auto const& d =
        *static_cast<Window::WindowData*>(glfwGetWindowUserPointer(w));
      auto code = static_cast<KeyCode>(key);
      auto modifiers = static_cast<Modifiers>(mods);
      if (action == GLFW_PRESS) {
        KeyPressedEvent ev{ code, modifiers };
        d.event_callback(ev);
      } else if (action == GLFW_RELEASE) {
        KeyReleasedEvent ev{ code, modifiers };
        d.event_callback(ev);
      }
    });
  glfwSetMouseButtonCallback(
    glfw_window, +[](GLFWwindow* w, int button, int action, int) {
      auto const& d =
        *static_cast<Window::WindowData*>(glfwGetWindowUserPointer(w));
      auto code = static_cast<MouseCode>(button);
      Logger::log_debug("Mouse button {} {}",
                        std::to_underlying(code),
                        action == GLFW_PRESS ? "pressed" : "released");
      if (action == GLFW_PRESS) {
        MouseButtonPressedEvent ev{ code };
        d.event_callback(ev);
      } else if (action == GLFW_RELEASE) {
        MouseButtonReleasedEvent ev{ code };
        d.event_callback(ev);
      }
    });
  glfwSetCursorPosCallback(
    glfw_window, +[](GLFWwindow* w, double x, double y) {
      auto const& d =
        *static_cast<Window::WindowData*>(glfwGetWindowUserPointer(w));
      MouseMovedEvent ev{ x, y };
      d.event_callback(ev);
    });
}

Window::Window(const WindowConfiguration& config)
  : user_data(std::make_unique<WindowData>())
{
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);

  if (glfwInit() == GLFW_FALSE) {
    assert(false && "Failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

  auto&& [width, height] = config.size.as<std::int32_t>();

  GLFWmonitor* selected_monitor = nullptr;
#ifdef USE_MONITOR_SELECTION
  if (config.monitor_name) {
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    for (int i = 0; i < count; ++i) {
      const char* name = glfwGetMonitorName(monitors[i]);
      if (name != nullptr && *config.monitor_name == name) {
        selected_monitor = monitors[i];
        break;
      }
    }
  }
#endif

  glfw_window =
    glfwCreateWindow(width, height, "Vulkan Window", selected_monitor, nullptr);
  if (!glfw_window) {
    glfwTerminate();
    assert(false && "Failed to create GLFW window");
  }

  if (config.x && config.y)
    glfwSetWindowPos(glfw_window, *config.x, *config.y);

  glfwSetWindowUserPointer(glfw_window, user_data.get());
  hookup_events();

  glfwSetDropCallback(
    glfw_window, +[](GLFWwindow*, int count, const char** paths) {
      std::lock_guard lock(Window::drag_drop_mutex);
      Window::drag_drop_files.clear();
      for (int i = 0; i < count; ++i) {
        Window::drag_drop_files.emplace_back(paths[i]);
      }
    });

  Input::initialise(glfw_window);
}

auto
Window::poll_drag_drop_files() -> std::span<const std::filesystem::path>
{
  std::lock_guard lock(drag_drop_mutex);

  drag_drop_cache = std::move(drag_drop_files);
  drag_drop_files.clear(); // clear input buffer

  return std::span<const std::filesystem::path>{ drag_drop_cache };
}

auto
Window::create_surface(const Core::Instance& instance) -> void
{
  if (glfwCreateWindowSurface(
        instance.raw(), glfw_window, nullptr, &vk_surface) != VK_SUCCESS) {
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
    assert(false && "Failed to create window surface");
  }
}

Window::~Window()
{
  cleanup();
}

auto
Window::destroy_surface(Badge<DynamicRendering::App>,
                        const Core::Instance& instance) -> void
{
  if (vk_surface != VK_NULL_HANDLE)
    vkb::destroy_surface(instance.vkb(), vk_surface);
}

auto
Window::cleanup() -> void
{
  if (glfw_window)
    save_window_config(config_path, glfw_window);

  if (glfw_window)
    glfwDestroyWindow(glfw_window);

  glfwTerminate();
  Input::destroy();
}

auto
Window::close() -> void
{
  glfwSetWindowShouldClose(glfw_window, GLFW_TRUE);
}

auto
Window::set_resize_flag(const bool flag) -> void
{
  user_data->framebuffer_resized = flag;
}

auto
Window::framebuffer_resized() const -> bool
{
  return user_data->framebuffer_resized;
}

auto
Window::framebuffer_size() const -> std::pair<std::uint32_t, std::uint32_t>
{
  int width, height;
  glfwGetFramebufferSize(glfw_window, &width, &height);
  return {
    static_cast<std::uint32_t>(width),
    static_cast<std::uint32_t>(height),
  };
}

auto
Window::should_close() const -> bool
{
  return glfwWindowShouldClose(glfw_window);
}

auto
Window::is_iconified() const -> bool
{
  return glfwGetWindowAttrib(glfw_window, GLFW_ICONIFIED) != 0;
}

auto
Window::is_minimized() const -> bool
{
  return glfwGetWindowAttrib(glfw_window, GLFW_VISIBLE) == 0;
}

auto
Window::set_event_callback(std::function<void(Event&)> callback) -> void
{
  user_data->event_callback = std::move(callback);
}

auto
Window::wait_for_events() const -> void
{
  glfwWaitEvents();
}