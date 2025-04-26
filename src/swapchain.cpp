#include "swapchain.hpp"

#include "gui_system.hpp"
#include "imgui.h"

auto
Swapchain::draw_frame(GUISystem& gui_system) -> void
{
  vkWaitForFences(
    device, 1, &in_flight_fences[frame_index], VK_TRUE, UINT64_MAX);
  vkResetFences(device, 1, &in_flight_fences[frame_index]);

  std::uint32_t image_index;
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

  gui_system.end_frame(cmd);

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

  vkQueueSubmit(queue, 1, &submit_info, in_flight_fences[frame_index]);

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished_semaphores[frame_index];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain;
  present_info.pImageIndices = &image_index;

  result = vkQueuePresentKHR(queue, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    recreate_swapchain();
  }

  frame_index = (frame_index + 1) % image_count;
}