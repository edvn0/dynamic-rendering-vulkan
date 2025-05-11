#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

#include "core/extent.hpp"

struct ImageConfiguration
{
  Extent2D extent;
  VkFormat format;
  uint32_t mip_levels = 1;
  uint32_t array_layers = 1;
  VkImageUsageFlags usage =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
  bool allow_in_ui{ true };
  bool is_cubemap{ false };
  std::string debug_name{};
};
