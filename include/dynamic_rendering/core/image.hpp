#pragma once

#include "config.hpp"
#include "device.hpp"
#include "image_configuration.hpp"

#include "allocator.hpp"
#include "sampler_manager.hpp"

#include <bit>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class Image
{
  static inline SamplerManager sampler_manager;

public:
  static auto init_sampler_cache(const Device& device) -> void
  {
    sampler_manager.initialize(device);
  }
  static auto destroy_samplers() -> void { sampler_manager.destroy_all(); }

  static auto create(const Device& device, const ImageConfiguration& config)
    -> std::unique_ptr<Image>
  {
    auto img = std::unique_ptr<Image>(new Image(device,
                                                config.format,
                                                config.extent,
                                                config.mip_levels,
                                                config.array_layers,
                                                config.usage,
                                                config.aspect,
                                                config.allow_in_ui,
                                                config.sample_count));
    img->recreate();
    return img;
  }

  ~Image() { destroy(); }

  auto get_view() const -> VkImageView { return default_view; }
  auto get_sampler() const -> const VkSampler&;
  template<typename T>
  auto get_texture_id() const -> T
  {
    return std::bit_cast<T>(texture_implementation_pointer);
  }
  auto get_mip_layer_view(uint32_t mip, uint32_t layer) const -> VkImageView
  {
    return mip_layer_views[layer * mip_levels + mip];
  }
  auto get_image() const -> VkImage { return image; }
  auto width() const -> uint32_t { return extent.width; }
  auto height() const -> uint32_t { return extent.height; }
  auto layers() const -> uint32_t { return array_layers; }
  auto mips() const -> uint32_t { return mip_levels; }
  auto get_descriptor_info() const -> const VkDescriptorImageInfo&
  {
    return image_descriptor_info;
  }

  auto resize(uint32_t new_width, uint32_t new_height)
  {
    extent.width = new_width;
    extent.height = new_height;
    std::cout << "Resizing image to: " << new_width << "x" << new_height
              << std::endl;
    recreate();
  }

private:
  Image(const Device& dev,
        VkFormat fmt,
        Extent2D ext,
        uint32_t mips,
        uint32_t layers,
        VkImageUsageFlags usage_flags,
        VkImageAspectFlags aspect_flags,
        bool allow,
        VkSampleCountFlags sc)
    : extent(ext)
    , mip_levels(mips)
    , array_layers(layers)
    , device(&dev)
    , format(fmt)
    , usage(usage_flags)
    , aspect(aspect_flags)
    , sample_count(sc)
    , allow_in_ui(allow)
  {
  }

  auto recreate() -> void;
  auto destroy() -> void;

  VkImage image{};
  VkImageView default_view{};
  VkSampler sampler{};
  std::vector<VkImageView> mip_layer_views{};
  VmaAllocation allocation{};
  Extent2D extent{};
  uint32_t mip_levels{};
  uint32_t array_layers{};
  const Device* device{};
  VkFormat format{};
  VkImageUsageFlags usage{};
  VkImageAspectFlags aspect{};
  VkSampleCountFlags sample_count{};             // For MSAA images.
  VkDescriptorImageInfo image_descriptor_info{}; // For descriptor sets.

  // For UI systems.
  bool allow_in_ui{ true }; // For UI systems
  std::uint64_t texture_implementation_pointer{};
};
