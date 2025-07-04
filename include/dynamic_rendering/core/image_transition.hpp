#pragma once

#include <vulkan/vulkan.h>

#include "core/image.hpp"

namespace CoreUtils {

struct image_barrier_info
{
  VkImage image;
  VkImageLayout old_layout;
  VkImageLayout new_layout;
  VkAccessFlags src_access_mask;
  VkAccessFlags dst_access_mask;
  VkPipelineStageFlags src_stage_mask;
  VkPipelineStageFlags dst_stage_mask;
  VkImageSubresourceRange subresource_range{
    VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
  };
};

inline void
cmd_transition_image(VkCommandBuffer cmd,
                     image_barrier_info const& info) noexcept
{
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = info.src_access_mask;
  barrier.dstAccessMask = info.dst_access_mask;
  barrier.oldLayout = info.old_layout;
  barrier.newLayout = info.new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = info.image;
  barrier.subresourceRange = info.subresource_range;

  vkCmdPipelineBarrier(cmd,
                       info.src_stage_mask,
                       info.dst_stage_mask,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &barrier);
}

inline auto
cmd_transition_to_color_attachment(
  VkCommandBuffer cmd,
  VkImage image,
  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
  VkPipelineStageFlags dst_stage =
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) noexcept -> void
{
  cmd_transition_image(cmd,
                       {
                         image,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         0u,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         src_stage,
                         dst_stage,
                       });
}

inline auto
cmd_transition_to_color_attachment(
  VkCommandBuffer cmd,
  Image& image,
  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
  VkPipelineStageFlags dst_stage =
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) noexcept -> void
{
  cmd_transition_image(cmd,
                       {
                         image.get_image(),
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         0u,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         src_stage,
                         dst_stage,
                       });
}

inline auto
cmd_transition_to_present(VkCommandBuffer cmd,
                          VkImage image,
                          VkPipelineStageFlags src_stage =
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VkPipelineStageFlags dst_stage =
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) noexcept
  -> void
{
  cmd_transition_image(cmd,
                       {
                         image,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_MEMORY_READ_BIT,
                         src_stage,
                         dst_stage,
                       });
}

inline auto
cmd_transition_to_shader_read(VkCommandBuffer cmd,
                              VkImage image,
                              VkPipelineStageFlags src_stage =
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VkPipelineStageFlags dst_stage =
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) noexcept
  -> void
{
  cmd_transition_image(cmd,
                       {
                         image,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT,
                         src_stage,
                         dst_stage,
                       });
}
inline auto
cmd_transition_to_shader_read(VkCommandBuffer cmd,
                              Image& image,
                              VkPipelineStageFlags src_stage =
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VkPipelineStageFlags dst_stage =
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) noexcept
  -> void
{
  cmd_transition_image(cmd,
                       {
                         image.get_image(),
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT,
                         src_stage,
                         dst_stage,
                       });
}

inline auto
cmd_transition_general_to_sampled(
  const VkCommandBuffer cmd,
  Image& image,
  const bool from_compute_queue = false) noexcept -> void
{
  constexpr VkPipelineStageFlags src_stage =
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  const VkPipelineStageFlags dst_stage =
    from_compute_queue
      ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT // fallback for compute queue
      : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

  cmd_transition_image(cmd,
                       {
                         image.get_image(),
                         VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_SHADER_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT,
                         src_stage,
                         dst_stage,
                       });
}

/// @brief Transition depth image from depth attachment to shader read layout.
/// @param cmd
/// @param image
/// @param src_stage
/// @param dst_stage
/// @return void
inline auto
cmd_transition_depth_to_shader_read(
  VkCommandBuffer cmd,
  VkImage image,
  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
  VkPipelineStageFlags dst_stage =
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) noexcept -> void
{
  cmd_transition_image(cmd,
                       {
                         image,
                         VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT,
                         src_stage,
                         dst_stage,
                         {
                           VK_IMAGE_ASPECT_DEPTH_BIT,
                           0,
                           1,
                           0,
                           1,
                         },
                       });
}

inline auto
cmd_transition_to_depth_attachment(
  VkCommandBuffer cmd,
  VkImage image,
  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
  VkPipelineStageFlags dst_stage =
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) noexcept -> void
{
  cmd_transition_image(cmd,
                       {
                         image,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                         0u,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         src_stage,
                         dst_stage,
                         {
                           VK_IMAGE_ASPECT_DEPTH_BIT,
                           0,
                           1,
                           0,
                           1,
                         },
                       });
}

inline auto
cmd_transition_to_general(
  const VkCommandBuffer cmd,
  const Image& image,
  const VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
  const VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
  const VkPipelineStageFlags dst_stage =
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) noexcept -> void
{
  cmd_transition_image(cmd,
                       {
                         image.get_image(),
                         old_layout,
                         VK_IMAGE_LAYOUT_GENERAL,
                         0u,
                         VK_ACCESS_SHADER_WRITE_BIT,
                         src_stage,
                         dst_stage,
                       });
}

} // namespace CoreUtils
