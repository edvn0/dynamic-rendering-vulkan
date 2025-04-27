#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

struct Extent2D
{
  std::uint32_t width{ 0 };
  std::uint32_t height{ 0 };

  Extent2D() = default;

  template<std::integral T>
  Extent2D(const T w, const T h)
    : width(static_cast<std::uint32_t>(w))
    , height(static_cast<std::uint32_t>(h))
  {
  }

  // Construct from std::pair<std::integral T, std::integral T>
  template<std::integral T>
  Extent2D(const std::pair<T, T>& pair)
    : width(static_cast<std::uint32_t>(pair.first))
    , height(static_cast<std::uint32_t>(pair.second))
  {
  }

  template<std::integral T>
  Extent2D(std::pair<T, T>&& pair)
    : width(static_cast<std::uint32_t>(pair.first))
    , height(static_cast<std::uint32_t>(pair.second))
  {
  }
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
  bool allow_in_ui{ true };
};
