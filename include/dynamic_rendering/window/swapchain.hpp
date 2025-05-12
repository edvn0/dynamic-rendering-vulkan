#pragma once

#include "core/config.hpp"
#include "core/forward.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>

class Swapchain
{
public:
  Swapchain(const Device& device, const Window& window);
  ~Swapchain();

  auto destroy() -> void;
  auto request_recreate() -> void;
  auto draw_frame(const GUISystem&) -> void;
  [[nodiscard]] auto get_frame_index() const -> std::uint32_t;
  auto get_image_count() const { return swapchain_image_count; }

private:
  void recreate_swapchain();
  void cleanup_swapchain();
  void create_command_pool();
  void allocate_command_buffers();
  void create_sync_objects();
  void create_swapchain(VkSwapchainKHR);

private:
  VkDevice device{};
  VkPhysicalDevice physical_device{};
  VkSurfaceKHR surface{};
  const Window* window{};
  VkQueue queue{};
  std::uint32_t queue_family_index{};

  std::uint32_t frame_index{ 0 };
  std::uint32_t swapchain_image_count{ 0 };

  VkSwapchainKHR swapchain{ VK_NULL_HANDLE };
  VkFormat swapchain_format{};
  VkExtent2D swapchain_extent{};

  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views_;

  VkCommandPool command_pool{ VK_NULL_HANDLE };

  std::array<VkCommandBuffer, frames_in_flight> command_buffers{};

  std::array<VkSemaphore, frames_in_flight> image_available_semaphores{};
  std::array<VkSemaphore, frames_in_flight> render_finished_semaphores{};
  std::array<VkFence, frames_in_flight> in_flight_fences{};

  [[nodiscard]] auto choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& available_formats) const
    -> VkSurfaceFormatKHR;
  [[nodiscard]] auto choose_present_mode(
    const std::vector<VkPresentModeKHR>& available_present_modes) const
    -> VkPresentModeKHR;
  [[nodiscard]] auto choose_extent(
    const VkSurfaceCapabilitiesKHR& capabilities) const -> VkExtent2D;
};