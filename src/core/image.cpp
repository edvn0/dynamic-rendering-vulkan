#include "core/image.hpp"

#include "pipeline/blueprint_configuration.hpp"

#include "imgui_impl_vulkan.h"

#include "core/allocator.hpp"
#include "core/fs.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image_transition.hpp"
#include "core/logger.hpp"
#include "renderer/material_bindings.hpp"

#include <ktx.h>
#include <ktxvulkan.h>
#include <stb_image.h>
#include <stb_image_resize2.h>

auto
Image::resize(const uint32_t new_width, const uint32_t new_height) -> void
{
  extent.width = new_width;
  extent.height = new_height;
  Logger::log_info(
    "Resizing image '{}' to: {}x{}", get_name(), new_width, new_height);
  recreate();

  // Reapply debug name after recreation
  if (!debug_name.empty()) {
    set_debug_name(debug_name);
  }
}

auto
Image::set_debug_name(const std::string_view name) -> void
{
  debug_name = name;
  if (image) {
    ::set_debug_name(
      *device, std::bit_cast<std::uint64_t>(image), VK_OBJECT_TYPE_IMAGE, name);
    const std::string view_name = std::string(name) + " (Default View)";
    ::set_debug_name(*device,
                     std::bit_cast<std::uint64_t>(default_view),
                     VK_OBJECT_TYPE_IMAGE_VIEW,
                     view_name);
    // Set debug name for each mip-layer view
    for (uint32_t layer = 0; layer < array_layers; ++layer) {
      for (uint32_t mip = 0; mip < mip_levels; ++mip) {
        auto view = mip_layer_views[layer * mip_levels + mip];
        std::string mip_layer_name = std::string(name) + " (Layer " +
                                     std::to_string(layer) + ", Mip " +
                                     std::to_string(mip) + ")";
        ::set_debug_name(*device,
                         std::bit_cast<std::uint64_t>(view),
                         VK_OBJECT_TYPE_IMAGE_VIEW,
                         mip_layer_name);
      }
    }

    // Set allocation name
    if (allocation) {
      ::set_vma_allocation_name(*device, allocation, name);
    }
  }
}

auto
Image::recreate() -> void
{
  destroy();

  const std::array access_queues = {
    device->compute_queue_family_index(),
    device->graphics_queue_family_index(),
  };

  VkImageCreateInfo image_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = is_cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
                        : VkImageCreateFlags{ 0 },
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = { extent.width, extent.height, 1 },
    .mipLevels = mip_levels,
    .arrayLayers = array_layers,
    .samples = static_cast<VkSampleCountFlagBits>(sample_count),
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_CONCURRENT,
    .queueFamilyIndexCount = static_cast<std::uint32_t>(access_queues.size()),
    .pQueueFamilyIndices = access_queues.data(),
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) {
    is_storage_image = true;
  }

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  if (vmaCreateImage(device->get_allocator().get(),
                     &image_info,
                     &alloc_info,
                     &image,
                     &allocation,
                     nullptr) != VK_SUCCESS) {
    assert(false && "Failed to create image");
    return;
  }

  const bool is_array = array_layers > 1;
  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
  if (is_cubemap) {
    view_type = array_layers > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
                                 : VK_IMAGE_VIEW_TYPE_CUBE;
  } else if (is_array) {
    view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  }

  VkImageViewCreateInfo default_view_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
     .flags = 0,
    .image = image,
    .viewType = view_type,
    .format = format,
    .components = {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    },
    .subresourceRange = {
        .aspectMask = aspect,
        .baseMipLevel = 0,
        .levelCount = mip_levels,
        .baseArrayLayer = 0,
        .layerCount = array_layers,
    },
  };

  vkCreateImageView(
    device->get_device(), &default_view_info, nullptr, &default_view);

  mip_layer_views.resize(mip_levels * array_layers);

  for (uint32_t layer = 0; layer < array_layers; ++layer) {
    for (uint32_t mip = 0; mip < mip_levels; ++mip) {
      VkImageViewCreateInfo view_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = mip,
                    .levelCount = 1,
                    .baseArrayLayer = layer,
                    .layerCount = 1,
                },
            };

      vkCreateImageView(device->get_device(),
                        &view_info,
                        nullptr,
                        &mip_layer_views[layer * mip_levels + mip]);
    }
  }

  VkSamplerCreateInfo sampler_info{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .magFilter = sampler_config.mag_filter,
    .minFilter = sampler_config.min_filter,
    .mipmapMode = sampler_config.mipmap_mode,
    .addressModeU = sampler_config.address_mode_u,
    .addressModeV = sampler_config.address_mode_v,
    .addressModeW = sampler_config.address_mode_w,
    .mipLodBias = sampler_config.mip_lod_bias,
    .anisotropyEnable = sampler_config.anisotropy_enable,
    .maxAnisotropy = sampler_config.max_anisotropy,
    .compareEnable = sampler_config.compare_enable,
    .compareOp = sampler_config.compare_op,
    .minLod = sampler_config.min_lod,
    .maxLod = sampler_config.max_lod > 0.0F
                ? sampler_config.max_lod
                : static_cast<float>(mip_levels - 1),
    .borderColor = sampler_config.border_color,
    .unnormalizedCoordinates = sampler_config.unnormalized_coordinates,
  };

  if (const Attachment attachment{ format }; attachment.is_depth()) {
    sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0f,
      .compareEnable = VK_TRUE,
      .compareOp = VK_COMPARE_OP_LESS,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      .unnormalizedCoordinates = VK_FALSE,
    };
  }

  sampler = sampler_manager.get_sampler(sampler_info);

  if (allow_in_ui && sample_count == VK_SAMPLE_COUNT_1_BIT) {
    texture_implementation_pointer =
      std::bit_cast<std::uint64_t>(ImGui_ImplVulkan_AddTexture(
        sampler, default_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
  }

  image_descriptor_info = VkDescriptorImageInfo{
    .sampler = sampler,
    .imageView = default_view,
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  if (is_storage_image) {
    image_descriptor_info = VkDescriptorImageInfo{
      .sampler = sampler,
      .imageView = default_view,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
  }

  if (initial_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
    OneTimeCommand command(*device, device->compute_queue());
    CoreUtils::cmd_transition_to_general(*command, *this);
  }
}

auto
Image::create(const Device& device, const ImageConfiguration& config)
  -> Assets::Pointer<Image>
{
  auto img = Assets::make_tracked<Image>(device,
                                         config.format,
                                         config.extent,
                                         config.mip_levels,
                                         config.array_layers,
                                         config.usage,
                                         config.aspect,
                                         config.initial_layout,
                                         config.sampler_config,
                                         config.allow_in_ui,
                                         config.sample_count,
                                         config.is_cubemap);

  if (!config.debug_name.empty()) {
    img->set_debug_name(config.debug_name);
  }

  img->recreate();
  return img;
}

auto
Image::get_sampler() const -> const VkSampler&
{
  return sampler;
}

auto
Image::destroy() -> void
{
  if (texture_implementation_pointer != 0) {
    ImGui_ImplVulkan_RemoveTexture(
      std::bit_cast<VkDescriptorSet>(texture_implementation_pointer));
    texture_implementation_pointer = 0;
  }

  for (const auto& view : mip_layer_views)
    if (view)
      vkDestroyImageView(device->get_device(), view, nullptr);
  mip_layer_views.clear();

  if (default_view) {
    vkDestroyImageView(device->get_device(), default_view, nullptr);
    default_view = nullptr;
  }

  if (image) {
    vmaDestroyImage(device->get_allocator().get(), image, allocation);
    image = nullptr;
    allocation = nullptr;
  }
}

auto
Image::upload_rgba(const std::span<const unsigned char> data) -> void
{
  if (data.empty())
    return;
  if (data.size_bytes() % 4 != 0)
    return;

  auto&& [cmd, pool] =
    device->create_one_time_command_buffer(device->graphics_queue());

  GPUBuffer staging{
    *device,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    true,
    "Image (RGBA) staging buffer",
  };
  staging.upload(data);

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = image,
      .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .src_access_mask = 0,
      .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 1 },
    });

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = { 0, 0, 0 };
  region.imageExtent = { extent.width, extent.height, 1 };

  vkCmdCopyBufferToImage(cmd,
                         staging.get(),
                         image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &region);

  uint32_t mip_width = extent.width;
  uint32_t mip_height = extent.height;

  for (uint32_t i = 1; i < mip_levels; ++i) {
    VkImageBlit blit{};
    blit.srcOffsets[1] = { static_cast<int32_t>(mip_width),
                           static_cast<int32_t>(mip_height),
                           1 };
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
    blit.dstOffsets[1] = {
      static_cast<int32_t>(mip_width > 1 ? mip_width / 2 : 1),
      static_cast<int32_t>(mip_height > 1 ? mip_height / 2 : 1),
      1
    };
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };

    CoreUtils::cmd_transition_image(
      cmd,
      {
        .image = image,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_TRANSFER_READ_BIT,
        .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 },
      });

    vkCmdBlitImage(cmd,
                   image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &blit,
                   VK_FILTER_LINEAR);

    CoreUtils::cmd_transition_image(
      cmd,
      {
        .image = image,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .src_access_mask = VK_ACCESS_TRANSFER_READ_BIT,
        .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
        .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 },
      });

    mip_width = mip_width > 1 ? mip_width / 2 : 1;
    mip_height = mip_height > 1 ? mip_height / 2 : 1;
  }

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = image,
      .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT,
                             mip_levels - 1,
                             1,
                             0,
                             1 },
    });

  device->flush(cmd, pool, device->graphics_queue());
}

auto
Image::load_from_file(const Device& device,
                      const std::string& path,
                      bool flip_y) -> Assets::Pointer<Image>
{
  stbi_set_flip_vertically_on_load(flip_y ? 1 : 0);

  int width;
  int height;
  int channels;
  stbi_uc* pixels =
    stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (!pixels) {
    Logger::log_error(
      "Failed to load image: {}. Reason: {}", path, stbi_failure_reason());
    return nullptr;
  }

  const auto mip_levels = 1 + static_cast<std::uint32_t>(
                                std::floor(std::log2(std::max(width, height))));

  const auto img_config = ImageConfiguration{
    .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) },
    .format = VK_FORMAT_R8G8B8A8_SRGB,
    .mip_levels =mip_levels,
    .array_layers = 1,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
    .sample_count = VK_SAMPLE_COUNT_1_BIT,
    .sampler_config = {
      .min_lod = 0.0f,
      .max_lod = static_cast<float>(mip_levels),
    },
    .allow_in_ui = true,
    .debug_name = std::filesystem::path{ path }.filename().string(),
  };

  auto image = create(device, img_config);

  image->upload_rgba(
    std::span{ pixels, static_cast<size_t>(width * height * STBI_rgb_alpha) });

  stbi_image_free(pixels);

  return image;
}

auto
Image::load_from_file(const Device& device,
                      const std::string& path,
                      const SampledTextureImageConfiguration& config,
                      bool flip_y) -> Assets::Pointer<Image>
{
  stbi_set_flip_vertically_on_load(flip_y ? 1 : 0);

  int width;
  int height;
  int channels;
  stbi_uc* pixels =
    stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (config.extent.has_value()) {
    auto method_choice = config.format == VK_FORMAT_R8G8B8A8_SRGB
                           ? stbir_resize_uint8_srgb
                           : stbir_resize_uint8_linear;

    // Resize using stb_image_resize
    stbi_uc* resized_pixels = nullptr;
    const auto new_width = config.extent->width;
    const auto new_height = config.extent->height;
    resized_pixels = method_choice(pixels,
                                   width,
                                   height,
                                   0,
                                   nullptr,
                                   new_width,
                                   new_height,
                                   0,
                                   stbir_pixel_layout::STBIR_RGBA);
    if (!resized_pixels) {
      Logger::log_error(
        "Failed to resize image: {}. Reason: {}", path, stbi_failure_reason());
      stbi_image_free(pixels);
      return nullptr;
    }
    width = new_width;
    height = new_height;
    stbi_image_free(pixels);
    pixels = resized_pixels;
  }

  if (!pixels) {
    Logger::log_error(
      "Failed to load image: {}. Reason: {}", path, stbi_failure_reason());
    return nullptr;
  }

  const auto mip_levels = 1 + static_cast<std::uint32_t>(
                                std::floor(std::log2(std::max(width, height))));

  SamplerConfiguration copy_sampler_config = config.sampler_config;
  copy_sampler_config.max_lod = copy_sampler_config.max_lod > 0.0f
                                  ? copy_sampler_config.max_lod
                                  : static_cast<float>(mip_levels - 1);
  const auto img_config = ImageConfiguration{
    .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) },
    .format = config.format,
    .mip_levels = mip_levels,
    .array_layers = 1,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
    .sample_count = VK_SAMPLE_COUNT_1_BIT,
    .sampler_config = copy_sampler_config,
    .allow_in_ui = true,
    .debug_name = std::filesystem::path{ path }.filename().string(),
  };

  auto image = create(device, img_config);

  image->upload_rgba(
    std::span{ pixels, static_cast<size_t>(width * height * STBI_rgb_alpha) });

  stbi_image_free(pixels);

  return image;
}

auto
Image::load_cubemap(const Device& device, const std::string& path)
  -> Assets::Pointer<Image>
{
  const auto image_path = assets_path() / "environment" / path;

  if (!std::filesystem::exists(image_path)) {
    Logger::log_error("Cubemap image not found: {}", image_path.string());
    return nullptr;
  }

  ktxTexture2* texture = nullptr;
  ktxResult result =
    ktxTexture2_CreateFromNamedFile(image_path.string().c_str(),
                                    KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                    &texture);

  if (result != KTX_SUCCESS || !texture) {
    auto error = ktxErrorString(result);
    Logger::log_error("Failed to load ktx2 cubemap: {}. Reason: {}",
                      image_path.string(),
                      error);
    return nullptr;
  }

  if (texture->numFaces != 6 || texture->numLayers != 1 ||
      texture->numLevels < 1) {
    Logger::log_error(
      "Invalid cubemap layout in KTX2: must be 6 faces, 1 layer");
    ktxTexture_Destroy(ktxTexture(texture));
    return nullptr;
  }

  const auto format = static_cast<VkFormat>(texture->vkFormat);
  const auto mip_levels = texture->numLevels;
  const auto width = texture->baseWidth;
  const auto height = texture->baseHeight;

  const auto config = ImageConfiguration{
    .extent = { width, height },
    .format = format,
    .mip_levels = mip_levels,
    .array_layers = 6,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
    .sample_count = VK_SAMPLE_COUNT_1_BIT,
    .allow_in_ui = false,
    .is_cubemap = true,
    .debug_name = std::filesystem::path{ path }.filename().string(),
  };

  auto image = create(device, config);

  std::vector<VkBufferImageCopy> regions;
  std::size_t total_size = 0;

  for (uint32_t level = 0; level < mip_levels; ++level) {
    for (uint32_t face = 0; face < 6; ++face) {
      ktx_size_t offset;
      const auto r =
        ktxTexture_GetImageOffset(ktxTexture(texture), level, 0, face, &offset);
      if (r == KTX_SUCCESS) {
        const auto face_size =
          ktxTexture_GetImageSize(ktxTexture(texture), level);
        total_size = std::max(total_size, offset + face_size);
      }
    }
  }

  GPUBuffer staging{
    device,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    true,
    "Staging buffer (Cubemap)",
  };
  staging.upload(std::span{
    static_cast<const std::uint8_t*>(ktxTexture_GetData(ktxTexture(texture))),
    total_size });

  for (uint32_t level = 0; level < mip_levels; ++level) {
    for (uint32_t face = 0; face < 6; ++face) {
      ktx_size_t offset;
      const auto ret =
        ktxTexture_GetImageOffset(ktxTexture(texture), level, 0, face, &offset);
      if (ret == KTX_SUCCESS) {
        regions.push_back({
          .bufferOffset = offset,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = level,
            .baseArrayLayer = face,
            .layerCount = 1,
          },
          .imageOffset = { 0, 0, 0 },
          .imageExtent = {
            std::max(1u, width >> level),
            std::max(1u, height >> level),
            1,
          },
        });
      }
    }
  }

  auto&& [cmd, pool] =
    device.create_one_time_command_buffer(device.graphics_queue());

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = image->get_image(),
      .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .src_access_mask = 0,
      .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 6 },
    });

  vkCmdCopyBufferToImage(cmd,
                         staging.get(),
                         image->get_image(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(regions.size()),
                         regions.data());

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = image->get_image(),
      .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 6 },
    });

  device.flush(cmd, pool, device.graphics_queue());

  ktxTexture_Destroy(ktxTexture(texture));

  return image;
}

auto
Image::upload_rgba_with_command_buffer(std::span<const unsigned char> data,
                                       VkCommandBuffer cmd) -> void
{
  if (data.empty())
    return;
  if (data.size_bytes() % 4 != 0)
    return;

  GPUBuffer staging{ *device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true };
  staging.upload(data);

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = image,
      .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .src_access_mask = 0,
      .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 1 },
    });

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = { 0, 0, 0 };
  region.imageExtent = { extent.width, extent.height, 1 };

  vkCmdCopyBufferToImage(cmd,
                         staging.get(),
                         image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &region);

  uint32_t mip_width = extent.width;
  uint32_t mip_height = extent.height;

  for (uint32_t i = 1; i < mip_levels; ++i) {
    VkImageBlit blit{};
    blit.srcOffsets[1] = { static_cast<int32_t>(mip_width),
                           static_cast<int32_t>(mip_height),
                           1 };
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
    blit.dstOffsets[1] = {
      static_cast<int32_t>(mip_width > 1 ? mip_width / 2 : 1),
      static_cast<int32_t>(mip_height > 1 ? mip_height / 2 : 1),
      1
    };
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };

    CoreUtils::cmd_transition_image(
      cmd,
      {
        .image = image,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_TRANSFER_READ_BIT,
        .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 },
      });

    vkCmdBlitImage(cmd,
                   image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &blit,
                   VK_FILTER_LINEAR);

    CoreUtils::cmd_transition_image(
      cmd,
      {
        .image = image,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .src_access_mask = VK_ACCESS_TRANSFER_READ_BIT,
        .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
        .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 },
      });

    mip_width = mip_width > 1 ? mip_width / 2 : 1;
    mip_height = mip_height > 1 ? mip_height / 2 : 1;
  }

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = image,
      .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT,
                             mip_levels - 1,
                             1,
                             0,
                             1 },
    });
}

auto
Image::load_from_file_with_staging(const Device& dev,
                                   const std::string& path,
                                   bool flip_y,
                                   bool ui_allow,
                                   VkCommandBuffer cmd,
                                   VkFormat image_format) -> ImageWithStaging
{
  stbi_set_flip_vertically_on_load(flip_y ? 1 : 0);

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels =
    stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (!pixels) {
    Logger::log_error(
      "Failed to load image: {}. Reason: {}", path, stbi_failure_reason());
    return {};
  }

  const size_t byte_count = static_cast<size_t>(width * height * 4);
  std::span<const std::uint8_t> rgba_data{ pixels, byte_count };

  const auto mip_levels =
    1 + static_cast<uint32_t>(std::floor(std::log2(std::max(width, height))));
  const auto config = ImageConfiguration{
    .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) },
    .format = image_format,
    .mip_levels =
      mip_levels,
    .array_layers = 1,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
    .sample_count = VK_SAMPLE_COUNT_1_BIT,
    .sampler_config = {
      .address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .min_lod = 0.0f,
      .max_lod =static_cast<float>(mip_levels),
    },
    .allow_in_ui = ui_allow,
    .debug_name = std::filesystem::path{ path }.filename().string(),
  };

  auto img = Image::create(dev, config);

  auto staging = std::make_unique<GPUBuffer>(
    dev, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, "Image (RGBA) staging buffer");
  staging->upload(rgba_data);
  img->record_upload_rgba_with_staging(cmd, *staging, width, height);

  stbi_image_free(pixels);

  return ImageWithStaging{
    .image = std::move(img),
    .staging = std::move(staging),
  };
}

auto
Image::is_cubemap_externally(const std::string& path)
  -> std::expected<bool, std::string>
{
  const auto image_path = assets_path() / "environment" / path;
  ktxTexture2* texture = nullptr;
  const ktxResult result = ktxTexture2_CreateFromNamedFile(
    image_path.string().c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &texture);

  if (result != KTX_SUCCESS || !texture) {
    return std::unexpected(
      std::format("Failed to open KTX: {}", ktxErrorString(result)));
  }

  bool valid = texture->numFaces == 6 && texture->numLayers == 1 &&
               texture->numLevels >= 1;

  ktxTexture_Destroy(ktxTexture(texture));
  return valid;
}
auto
Image::init_sampler_cache(const Device& device) -> void
{
  sampler_manager.initialize(device);
}
auto
Image::destroy_samplers() -> void
{
  sampler_manager.destroy_all();
}

auto
Image::record_upload_rgba_with_staging(VkCommandBuffer cmd,
                                       const GPUBuffer& staging,
                                       uint32_t width,
                                       uint32_t height) const -> void
{
  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = image,
      .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .src_access_mask = 0,
      .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 1 },
    });

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = { 0, 0, 0 };
  region.imageExtent = { width, height, 1 };

  vkCmdCopyBufferToImage(cmd,
                         staging.get(),
                         image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &region);

  uint32_t mip_width = width;
  uint32_t mip_height = height;

  for (uint32_t i = 1; i < mip_levels; ++i) {
    VkImageBlit blit{};
    blit.srcOffsets[1] = { static_cast<int32_t>(mip_width),
                           static_cast<int32_t>(mip_height),
                           1 };
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
    blit.dstOffsets[1] = {
      static_cast<int32_t>(mip_width > 1 ? mip_width / 2 : 1),
      static_cast<int32_t>(mip_height > 1 ? mip_height / 2 : 1),
      1
    };
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };

    CoreUtils::cmd_transition_image(
      cmd,
      {
        .image = image,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_TRANSFER_READ_BIT,
        .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 },
      });

    vkCmdBlitImage(cmd,
                   image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &blit,
                   VK_FILTER_LINEAR);

    CoreUtils::cmd_transition_image(
      cmd,
      {
        .image = image,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .src_access_mask = VK_ACCESS_TRANSFER_READ_BIT,
        .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
        .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 },
      });

    mip_width = mip_width > 1 ? mip_width / 2 : 1;
    mip_height = mip_height > 1 ? mip_height / 2 : 1;
  }

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = image,
      .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT,
                             mip_levels - 1,
                             1,
                             0,
                             1 },
    });
}