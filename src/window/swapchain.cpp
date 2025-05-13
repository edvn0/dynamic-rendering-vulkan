#include "window/swapchain.hpp"

#include "core/image_transition.hpp"
#include "window/gui_system.hpp"
#include "window/window.hpp"

#include "core/device.hpp"

#include <imgui.h>
#include <iostream>
#include <tracy/Tracy.hpp>

Swapchain::Swapchain(const Device& device_ref, const Window& window_ref)
  : device(device_ref.get_device())
  , physical_device(device_ref.get_physical_device())
  , surface(window_ref.surface())
  , window(&window_ref)
  , queue(device_ref.graphics_queue())
  , queue_family_index(device_ref.graphics_queue_family_index())
{
  create_swapchain(VK_NULL_HANDLE);
  create_command_pool();
  allocate_command_buffers();
  create_sync_objects();
}

auto
Swapchain::draw_frame(const GUISystem& gui_system) -> void
{
  ZoneScopedN("Swapchain::draw_frame");
  vkWaitForFences(
    device, 1, &in_flight_fences[frame_index], VK_TRUE, UINT64_MAX);
  vkResetFences(device, 1, &in_flight_fences[frame_index]);

  // Acquire an image from the swapchain
  std::uint32_t image_index{};
  {
    ZoneScopedN("Acquire image");
    auto result = vkAcquireNextImageKHR(device,
                                        swapchain,
                                        UINT64_MAX,
                                        image_available_semaphores[frame_index],
                                        VK_NULL_HANDLE,
                                        &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreate_swapchain();
      return;
    }
  }

  // Reset and begin recording command buffer
  VkCommandBuffer cmd = command_buffers[frame_index];
  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &begin_info);

#define MEMORY_BARRIER
#ifdef MEMORY_BARRIER
  CoreUtils::cmd_transition_to_color_attachment(cmd,
                                                swapchain_images[image_index]);
#endif

  const VkRenderingAttachmentInfo color_attachment_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = this->swapchain_image_views_[image_index],
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .resolveMode = VK_RESOLVE_MODE_NONE_KHR,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue =
          {
              .color = {{0.0f, 0.0f, 0.0f, 1.0f}},
          },
  };

  const auto render_info = VkRenderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = swapchain_extent,
          },
      .layerCount = 1,
      .viewMask = 0,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info,
      .pDepthAttachment = nullptr,
      .pStencilAttachment = nullptr,
  };

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = static_cast<float>(swapchain_extent.height);
  viewport.width = static_cast<float>(swapchain_extent.width);
  viewport.height = -static_cast<float>(swapchain_extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  vkCmdBeginRendering(cmd, &render_info);

  {
    ZoneScopedN("Render GUI (From Swapchain)");
    gui_system.end_frame(cmd);
  }
  vkCmdEndRendering(cmd);

#ifdef MEMORY_BARRIER
  CoreUtils::cmd_transition_to_present(cmd, swapchain_images[image_index]);
#endif

  vkEndCommandBuffer(cmd);

  const std::array buffers = { cmd };
  const std::array wait_semaphores = {
    image_available_semaphores[frame_index],
  };
  const std::array signal_semaphores = {
    render_finished_semaphores[frame_index],
  };
  const std::array<VkPipelineStageFlags, 1> wait_stages = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  };

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount =
    static_cast<std::uint32_t>(wait_semaphores.size());
  submit_info.pWaitSemaphores = wait_semaphores.data();
  submit_info.pWaitDstStageMask = wait_stages.data();
  submit_info.commandBufferCount = static_cast<std::uint32_t>(buffers.size());
  submit_info.pCommandBuffers = buffers.data();
  submit_info.signalSemaphoreCount =
    static_cast<std::uint32_t>(signal_semaphores.size());
  submit_info.pSignalSemaphores = signal_semaphores.data();

  {
    ZoneScopedN("Submit");
    vkQueueSubmit(queue, 1, &submit_info, in_flight_fences[frame_index]);
  }
  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished_semaphores[frame_index];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain;
  present_info.pImageIndices = &image_index;

  {
    ZoneScopedN("Present");
    auto result = vkQueuePresentKHR(queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      recreate_swapchain();
    }
  }

  frame_index = (frame_index + 1) % frames_in_flight;
}

auto
Swapchain::recreate_swapchain() -> void
{
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  while (width == 0 || height == 0) {
    auto&& [w, h] = window->framebuffer_size();
    width = w;
    height = h;
    window->wait_for_events();
  }

  std::cout << "Recreating swapchain..." << std::endl;

  vkDeviceWaitIdle(device);

  VkSwapchainKHR old_swapchain = swapchain;

  for (auto view : swapchain_image_views_) {
    vkDestroyImageView(device, view, nullptr);
  }
  swapchain_image_views_.clear();

  create_swapchain(old_swapchain);

  if (old_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, old_swapchain, nullptr);
  }
}

Swapchain::~Swapchain()
{
  if (!swapchain) {
    return;
  }

  vkDeviceWaitIdle(device);
  cleanup_swapchain();
  vkDestroyCommandPool(device, command_pool, nullptr);
  for (std::uint32_t i = 0; i < frames_in_flight; ++i) {
    vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
    vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
    vkDestroyFence(device, in_flight_fences[i], nullptr);
  }
}

auto
Swapchain::get_frame_index() const -> std::uint32_t
{
  return frame_index;
}

auto
Swapchain::destroy() -> void
{
  vkDeviceWaitIdle(device);
  cleanup_swapchain();
  vkDestroyCommandPool(device, command_pool, nullptr);
  for (std::uint32_t i = 0; i < frames_in_flight; ++i) {
    vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
    vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
    vkDestroyFence(device, in_flight_fences[i], nullptr);
  }

  swapchain = VK_NULL_HANDLE;
  command_pool = VK_NULL_HANDLE;
}

auto
Swapchain::request_recreate() -> void
{
  recreate_swapchain();
}

auto
Swapchain::cleanup_swapchain() -> void
{
  for (auto view : swapchain_image_views_) {
    vkDestroyImageView(device, view, nullptr);
  }
  swapchain_image_views_.clear();

  if (swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
  }
}

auto
Swapchain::create_command_pool() -> void
{
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = queue_family_index;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);
}

auto
Swapchain::allocate_command_buffers() -> void
{
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = frames_in_flight;
  vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data());
}

auto
Swapchain::create_sync_objects() -> void
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

  for (std::uint32_t i = 0; i < frames_in_flight; ++i) {
    vkCreateSemaphore(
      device, &semaphore_info, nullptr, &image_available_semaphores[i]);
    vkCreateSemaphore(
      device, &semaphore_info, nullptr, &render_finished_semaphores[i]);
    vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]);
  }
}

void
Swapchain::create_swapchain(VkSwapchainKHR old_swapchain)
{
  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);

  swapchain_extent = choose_extent(caps);

  Logger::log_info(
    "Swapchain extent: {}x{}", swapchain_extent.width, swapchain_extent.height);

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

  // Ensure we have at least max_swapchain_image_count images, but respect
  // device limits
  std::uint32_t desired_image_count =
    std::max(caps.minImageCount, max_swapchain_image_count);
  if (caps.maxImageCount > 0 && desired_image_count > caps.maxImageCount) {
    desired_image_count = caps.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface;
  create_info.minImageCount = desired_image_count;
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
  create_info.oldSwapchain = old_swapchain;

  VkResult result =
    vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain);
  if (result != VK_SUCCESS) {
    assert(false && "Failed to create swapchain");
  }

  // Get actual swapchain image count
  std::uint32_t count;
  vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
  swapchain_image_count = count;
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

    if (vkCreateImageView(
          device, &view_info, nullptr, &swapchain_image_views_[i]) !=
        VK_SUCCESS) {
      assert(false);
    }
  }
}

[[nodiscard]] auto
Swapchain::choose_surface_format(
  const std::vector<VkSurfaceFormatKHR>& available_formats) const
  -> VkSurfaceFormatKHR
{
  for (const auto& format : available_formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  return available_formats[0];
}

[[nodiscard]] auto
Swapchain::choose_present_mode(
  const std::vector<VkPresentModeKHR>& available_present_modes) const
  -> VkPresentModeKHR
{
  for (const auto& mode : available_present_modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return mode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

[[nodiscard]] auto
Swapchain::choose_extent(const VkSurfaceCapabilitiesKHR& capabilities) const
  -> VkExtent2D
{
  if (capabilities.currentExtent.width !=
      std::numeric_limits<std::uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  auto [width, height] = window->framebuffer_size();

  width = std::clamp(width,
                     capabilities.minImageExtent.width,
                     capabilities.maxImageExtent.width);
  height = std::clamp(height,
                      capabilities.minImageExtent.height,
                      capabilities.maxImageExtent.height);

  return { static_cast<std::uint32_t>(width),
           static_cast<std::uint32_t>(height) };
}