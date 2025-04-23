// image_transition.hpp
#pragma once

#include <vulkan/vulkan.h>

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

} // namespace CoreUtils
