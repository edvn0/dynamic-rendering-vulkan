#pragma once

#include "config.hpp"
#include "device.hpp"
#include "image_transition.hpp"
#include "window.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

class GUISystem;

class Swapchain
{
public:
  Swapchain(const Device& device, const Window& window)
    : device(device.get_device())
    , physical_device(device.get_physical_device())
    , surface(window.surface())
    , queue(device.graphics_queue())
    , queue_family_index(device.graphics_queue_family_index())
  {
    create_swapchain();
    create_command_pool();
    allocate_command_buffers();
    create_sync_objects();
  }

  auto destroy() -> void
  {
    cleanup_swapchain();
    vkDestroyCommandPool(device, command_pool, nullptr);
    for (std::uint32_t i = 0; i < image_count; ++i) {
      vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
      vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
      vkDestroyFence(device, in_flight_fences[i], nullptr);
    }
  }

  auto request_recreate(Window& w) -> void { recreate_swapchain(w); }
  auto draw_frame(Window& w, GUISystem& gui_system) -> void;
  auto get_frame_index() const -> std::uint32_t { return frame_index; }

private:
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkSurfaceKHR surface;
  VkQueue queue;
  std::uint32_t queue_family_index;
  std::uint32_t frame_index = 0;

  VkSwapchainKHR swapchain{ VK_NULL_HANDLE };
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;

  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views_;

  VkCommandPool command_pool{ VK_NULL_HANDLE };
  std::array<VkCommandBuffer, image_count> command_buffers{};

  std::array<VkSemaphore, image_count> image_available_semaphores{};
  std::array<VkSemaphore, image_count> render_finished_semaphores{};
  std::array<VkFence, image_count> in_flight_fences{};

  auto recreate_swapchain(Window& window) -> void
  {
    int width = 0;
    int height = 0;
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window.window(), &width, &height);
      glfwWaitEvents();
    }

    std::cout << "Recreating swapchain..." << std::endl;

    vkDeviceWaitIdle(device);
    cleanup_swapchain();
    create_swapchain();
  }

  auto cleanup_swapchain() -> void
  {
    for (auto view : swapchain_image_views_) {
      vkDestroyImageView(device, view, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  }

  auto create_command_pool() -> void
  {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);
  }

  auto allocate_command_buffers() -> void
  {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = image_count;
    vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data());
  }

  auto create_sync_objects() -> void
  {
    VkSemaphoreCreateInfo semaphore_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
    };
    VkFenceCreateInfo fence_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (std::uint32_t i = 0; i < image_count; ++i) {
      vkCreateSemaphore(
        device, &semaphore_info, nullptr, &image_available_semaphores[i]);
      vkCreateSemaphore(
        device, &semaphore_info, nullptr, &render_finished_semaphores[i]);
      vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]);
    }
  }

  auto choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& available_formats) const
    -> VkSurfaceFormatKHR
  {
    for (auto& format : available_formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
          format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return format;
      }
    }
    return available_formats[0];
  }

  auto choose_present_mode(
    const std::vector<VkPresentModeKHR>& available_present_modes) const
    -> VkPresentModeKHR
  {
    for (auto& mode : available_present_modes) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  auto choose_extent(const VkSurfaceCapabilitiesKHR& capabilities) const
    -> VkExtent2D
  {
    if (capabilities.currentExtent.width != UINT32_MAX) {
      return capabilities.currentExtent;
    }
    VkExtent2D actual_extent = capabilities.currentExtent;
    actual_extent.width = std::clamp(actual_extent.width,
                                     capabilities.minImageExtent.width,
                                     capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(actual_extent.height,
                                      capabilities.minImageExtent.height,
                                      capabilities.maxImageExtent.height);
    return actual_extent;
  }

  void create_swapchain()
  {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);

    swapchain_extent = choose_extent(caps);

    std::cout << "Swapchain extent: " << swapchain_extent.width << "x"
              << swapchain_extent.height << std::endl;

    std::uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical_device, surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical_device, surface, &format_count, formats.data());
    auto surface_format = choose_surface_format(formats);
    swapchain_format = surface_format.format;

    std::uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device, surface, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device, surface, &present_mode_count, present_modes.data());
    auto present_mode = choose_present_mode(present_modes);

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = swapchain_format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = swapchain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;

    vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain);

    std::uint32_t count;
    vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
    swapchain_images.resize(count);
    vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data());

    swapchain_image_views_.resize(count);
    for (size_t i = 0; i < count; ++i) {
      VkImageViewCreateInfo view_info{};
      view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view_info.image = swapchain_images[i];
      view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view_info.format = swapchain_format;
      view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view_info.subresourceRange.levelCount = 1;
      view_info.subresourceRange.layerCount = 1;

      vkCreateImageView(
        device, &view_info, nullptr, &swapchain_image_views_[i]);
    }
  }
};
