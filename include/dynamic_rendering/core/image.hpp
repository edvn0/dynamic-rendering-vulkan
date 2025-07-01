#pragma once

#include "assets/asset_allocator.hpp"
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
  Image() = default;
  Image(const Device& dev,
        const VkFormat fmt,
        const Extent2D ext,
        const std::uint32_t mips,
        const std::uint32_t layers,
        const VkImageUsageFlags usage_flags,
        const VkImageAspectFlags aspect_flags,
        const VkImageLayout layout,
        const SamplerConfiguration& sample_conf,
        const bool allow,
        const VkSampleCountFlags sc,
        const bool cube)
    : extent(ext)
    , mip_levels(mips)
    , array_layers(layers)
    , device(&dev)
    , format(fmt)
    , usage(usage_flags)
    , aspect(aspect_flags)
    , initial_layout(layout)
    , sample_count(sc)
    , sampler_config(sample_conf)
    , is_cubemap(cube)
    , allow_in_ui(allow)
  {
  }
  ~Image() { destroy(); }

  [[nodiscard]] auto get_view() const -> VkImageView { return default_view; }
  [[nodiscard]] auto get_sampler() const -> const VkSampler&;
  template<typename T>
  [[nodiscard]] auto get_texture_id() const -> std::optional<T>
  {
    if (!texture_implementation_pointer)
      return std::nullopt;
    return std::bit_cast<T>(texture_implementation_pointer);
  }
  [[nodiscard]] auto get_mip_layer_view(const std::uint32_t mip,
                                        const std::uint32_t layer) const
    -> VkImageView
  {
    return mip_layer_views[layer * mip_levels + mip];
  }
  [[nodiscard]] auto get_image() const -> VkImage { return image; }
  [[nodiscard]] auto width() const -> std::uint32_t { return extent.width; }
  [[nodiscard]] auto height() const -> std::uint32_t { return extent.height; }
  [[nodiscard]] auto size() const -> glm::uvec2
  {
    return { width(), height() };
  }
  [[nodiscard]] auto layers() const -> std::uint32_t { return array_layers; }
  [[nodiscard]] auto mips() const -> std::uint32_t { return mip_levels; }
  [[nodiscard]] auto get_aspect_ratio() const
  {
    return static_cast<float>(extent.width) / static_cast<float>(extent.height);
  }
  [[nodiscard]] auto get_descriptor_info() const -> const VkDescriptorImageInfo&
  {
    return image_descriptor_info;
  }
  [[nodiscard]] auto get_name() const -> const std::string&
  {
    return debug_name;
  }
  [[nodiscard]] auto is_used_as_storage() const -> bool
  {
    return is_storage_image;
  }

  auto resize(std::uint32_t new_width, std::uint32_t new_height) -> void;

  /// @brief Resize the image to the current extent.
  auto resize() -> void { resize(extent.width, extent.height); }

  auto set_debug_name(std::string_view name) -> void;
  auto upload_rgba(std::span<const unsigned char>) -> void;

  static auto load_from_file(const Device&,
                             const std::string&,
                             bool flip_y = true) -> Assets::Pointer<Image>;
  static auto load_from_file(const Device&,
                             const std::string&,
                             const SampledTextureImageConfiguration&,
                             bool flip_y = true) -> Assets::Pointer<Image>;
  static auto load_cubemap(const Device&, const std::string&)
    -> Assets::Pointer<Image>;

  struct ImageWithStaging
  {
    Assets::Pointer<Image> image;
    std::unique_ptr<GPUBuffer> staging;
  };
  static auto load_from_file_with_staging(
    const Device& dev,
    const std::string& path,
    bool flip_y,
    bool ui_allow,
    VkCommandBuffer cmd,
    VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB) -> ImageWithStaging;
  static auto is_cubemap_externally(const std::string& path)
    -> std::expected<bool, std::string>;
  static auto init_sampler_cache(const Device& device) -> void;
  static auto destroy_samplers() -> void;
  static auto create(const Device& device, const ImageConfiguration& config)
    -> Assets::Pointer<Image>;

private:
  auto recreate() -> void;
  auto destroy() -> void;

  VkImage image{};
  VkImageView default_view{};
  VkSampler sampler{};
  std::vector<VkImageView> mip_layer_views{};
  VmaAllocation allocation{};
  Extent2D extent{};
  std::uint32_t mip_levels{};
  std::uint32_t array_layers{};
  const Device* device{};
  VkFormat format{};
  VkImageUsageFlags usage{};
  VkImageAspectFlags aspect{};
  VkImageLayout initial_layout{ VK_IMAGE_LAYOUT_UNDEFINED };
  VkSampleCountFlags sample_count{};             // For MSAA images.
  VkDescriptorImageInfo image_descriptor_info{}; // For descriptor sets.
  SamplerConfiguration sampler_config{};
  bool is_cubemap{ false };
  std::string debug_name{};
  bool is_storage_image{ false };

  // For UI systems.
  bool allow_in_ui{ true }; // For UI systems
  std::uint64_t texture_implementation_pointer{};

  auto upload_rgba_with_command_buffer(std::span<const unsigned char>,
                                       VkCommandBuffer) -> void;
  auto record_upload_rgba_with_staging(VkCommandBuffer cmd,
                                       const GPUBuffer& staging,
                                       std::uint32_t width,
                                       std::uint32_t height) const -> void;
};
