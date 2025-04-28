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

private:
  void recreate_swapchain();
  void cleanup_swapchain();
  void create_command_pool();
  void allocate_command_buffers();
  void create_sync_objects();
  void create_swapchain(VkSwapchainKHR old_swapchain);

  [[nodiscard]] auto choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& available_formats) const
    -> VkSurfaceFormatKHR;
  [[nodiscard]] auto choose_present_mode(
    const std::vector<VkPresentModeKHR>& available_present_modes) const
    -> VkPresentModeKHR;
  [[nodiscard]] auto choose_extent(
    const VkSurfaceCapabilitiesKHR& capabilities) const -> VkExtent2D;

private:
  VkDevice device{};
  VkPhysicalDevice physical_device{};
  VkSurfaceKHR surface{};
  const Window* window{};
  VkQueue queue{};
  std::uint32_t queue_family_index{};

  std::uint32_t frame_index{ 0 };

  VkSwapchainKHR swapchain{ VK_NULL_HANDLE };
  VkFormat swapchain_format{};
  VkExtent2D swapchain_extent{};

  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views_;

  VkCommandPool command_pool{ VK_NULL_HANDLE };
  frame_array<VkCommandBuffer> command_buffers{}; // assuming triple buffering

  frame_array<VkSemaphore> image_available_semaphores{};
  frame_array<VkSemaphore> render_finished_semaphores{};
  frame_array<VkFence> in_flight_fences{};
};