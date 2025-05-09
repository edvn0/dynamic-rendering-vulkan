#pragma once

#include "core/config.hpp"
#include "core/device.hpp"
#include "core/image_configuration.hpp"
#include "core/logger.hpp"
#include "debug_utils.hpp"

#include "core/allocator.hpp"
#include "core/sampler_manager.hpp"

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

    if (!config.debug_name.empty()) {
      img->set_debug_name(config.debug_name);
    }

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
  auto get_mip_layer_view(const uint32_t mip, const uint32_t layer) const
    -> VkImageView
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

  auto resize(const uint32_t new_width, const uint32_t new_height)
  {
    extent.width = new_width;
    extent.height = new_height;
    Logger::log_info("Resizing image to: {}x{}", new_width, new_height);
    recreate();

    // Reapply debug name after recreation
    if (!debug_name.empty()) {
      set_debug_name(debug_name);
    }
  }
  auto set_debug_name(std::string_view name) -> void;
  auto get_name() const -> const std::string& { return debug_name; }

  auto upload_rgba(std::span<const unsigned char>) -> void;

  static auto load_from_file(const Device&,
                             const std::string&,
                             bool flip_y = true) -> std::unique_ptr<Image>;

private:
  Image(const Device& dev,
        const VkFormat fmt,
        const Extent2D ext,
        const uint32_t mips,
        const uint32_t layers,
        const VkImageUsageFlags usage_flags,
        const VkImageAspectFlags aspect_flags,
        const bool allow,
        const VkSampleCountFlags sc)
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
  std::string debug_name{};

  // For UI systems.
  bool allow_in_ui{ true }; // For UI systems
  std::uint64_t texture_implementation_pointer{};
};
