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
    glfw_window, [](GLFWwindow* w, int key, int, int action, int) {
      auto const& d =
        *static_cast<Window::WindowData*>(glfwGetWindowUserPointer(w));
      auto code = static_cast<KeyCode>(key);
      if (action == GLFW_PRESS) {
        KeyPressedEvent ev{ code };
        d.event_callback(ev);
      } else if (action == GLFW_RELEASE) {
        KeyReleasedEvent ev{ code };
        d.event_callback(ev);
      }
    });
  glfwSetMouseButtonCallback(
    glfw_window, +[](GLFWwindow* w, int button, int action, int) {
      auto const& d =
        *static_cast<Window::WindowData*>(glfwGetWindowUserPointer(w));
      if (action == GLFW_PRESS) {
        MouseButtonPressedEvent ev{ button };
        d.event_callback(ev);
      } else if (action == GLFW_RELEASE) {
        MouseButtonReleasedEvent ev{ button };
        d.event_callback(ev);
      }
    });

#ifdef DISABLE_PERFORMANCE
  glfwSetCursorPosCallback(
    glfw_window, +[](GLFWwindow* w, double x, double y) {
      auto const& d =
        *static_cast<Window::WindowData*>(glfwGetWindowUserPointer(w));
      MouseMovedEvent ev{ x, y };
      d.event_callback(ev);
    });
#endif
}

Window::Window()
  : user_data(std::make_unique<WindowData>())
{
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);

  if (glfwInit() == GLFW_FALSE) {
    assert(false && "Failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  glfw_window = glfwCreateWindow(1280, 720, "Vulkan Window", nullptr, nullptr);
  if (!glfw_window) {
    glfwTerminate();
    assert(false && "Failed to create GLFW window");
  }

  glfwSetWindowUserPointer(glfw_window, user_data.get());
  hookup_events();

  Input::initialise(glfw_window);
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
Window::destroy(const Core::Instance& instance) -> void
{
  if (vk_surface != VK_NULL_HANDLE)
    vkb::destroy_surface(instance.vkb(), vk_surface);
  cleanup();
}

auto
Window::cleanup() -> void
{
  if (destroyed)
    return;

  if (glfw_window)
    glfwDestroyWindow(glfw_window);

  glfwTerminate();
  destroyed = true;

  Input::destroy();
}

auto
Window::close() -> void
{
  glfwSetWindowShouldClose(glfw_window, GLFW_TRUE);
}

auto
Window::set_resize_flag(bool flag) -> void
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
Window::set_event_callback(std::function<void(Event&)> callback) -> void
{
  user_data->event_callback = std::move(callback);
}

auto
Window::wait_for_events() const -> void
{
  glfwWaitEvents();
}