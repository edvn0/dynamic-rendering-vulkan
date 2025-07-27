#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>

#include "core/event_system.hpp"
#include "core/extent.hpp"
#include "core/forward.hpp"
#include "core/util.hpp"
#include "window/window_configuration.hpp"

extern "C"
{
  struct GLFWwindow;
}

class Window
{
public:
  ~Window();

  auto create_surface(const Core::Instance&) -> void;
  auto window() const -> const auto* { return glfw_window; }
  auto window() -> auto* { return glfw_window; }
  auto surface() const -> VkSurfaceKHR { return vk_surface; }
  auto framebuffer_resized() const -> bool;
  auto framebuffer_size() const -> std::pair<std::uint32_t, std::uint32_t>;
  auto close() -> void;
  auto set_resize_flag(bool flag) -> void;
  auto should_close() const -> bool;
  auto is_iconified() const -> bool;
  auto is_minimized() const -> bool;
  auto wait_for_events() const -> void;

  auto destroy_surface(Badge<DynamicRendering::App>, const Core::Instance&)
    -> void;

  auto set_event_callback(std::function<void(Event&)> callback) -> void;
  static auto create(const WindowConfiguration&,
                     std::filesystem::path = get_default_config_path())
    -> std::unique_ptr<Window>;

  static auto poll_drag_drop_files() -> std::span<const std::filesystem::path>;

private:
  GLFWwindow* glfw_window{ nullptr };
  VkSurfaceKHR vk_surface{ VK_NULL_HANDLE };
  struct WindowData;
  std::unique_ptr<WindowData> user_data;
  std::filesystem::path config_path;

  static inline std::mutex drag_drop_mutex;
  static inline std::vector<std::filesystem::path> drag_drop_files;
  static inline std::vector<std::filesystem::path> drag_drop_cache;

  explicit Window(const WindowConfiguration&);
  auto cleanup() -> void;
  auto hookup_events() -> void;
};