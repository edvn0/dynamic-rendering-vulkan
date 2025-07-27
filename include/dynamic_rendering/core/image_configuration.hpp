#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

#include "core/extent.hpp"

struct SamplerConfiguration
{
  VkFilter mag_filter = VK_FILTER_LINEAR;
  VkFilter min_filter = VK_FILTER_LINEAR;
  VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  float mip_lod_bias = 0.0f;
  VkBool32 anisotropy_enable = VK_FALSE;
  float max_anisotropy = 1.0f;
  VkBool32 compare_enable = VK_FALSE;
  VkCompareOp compare_op = VK_COMPARE_OP_LESS;
  float min_lod = 0.0f;
  float max_lod = 0.0f;
  VkBorderColor border_color = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  VkBool32 unnormalized_coordinates = VK_FALSE;
};

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
  VkImageLayout initial_layout{ VK_IMAGE_LAYOUT_UNDEFINED };
  SamplerConfiguration sampler_config;
  bool allow_in_ui{ true };
  bool is_cubemap{ false };
  std::string debug_name{};
};

struct SampledTextureImageConfiguration
{
  VkFormat format{ VK_FORMAT_R8G8B8A8_SRGB };
  SamplerConfiguration sampler_config{
    .mag_filter = VK_FILTER_LINEAR,
    .min_filter = VK_FILTER_LINEAR,
    .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT,
  };
  // Downsamples the image on load time.
  std::optional<VkExtent2D> extent{ std::nullopt };
};