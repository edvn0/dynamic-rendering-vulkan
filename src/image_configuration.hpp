#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

struct Extent2D {
  std::uint32_t width;
  std::uint32_t height;
};

struct ImageConfiguration {
  Extent2D extent;
  VkFormat format;
};
