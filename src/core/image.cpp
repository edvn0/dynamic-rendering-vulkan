#include "core/image.hpp"

#include "pipeline/blueprint_configuration.hpp"

#include "imgui_impl_vulkan.h"

#include "core/allocator.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image_transition.hpp"
#include "renderer/material_bindings.hpp"

#include <stb_image.h>

auto
Image::set_debug_name(const std::string_view name) -> void
{
  debug_name = name;
  if (image) {
    ::set_debug_name(
      *device, std::bit_cast<std::uint64_t>(image), VK_OBJECT_TYPE_IMAGE, name);
    std::string view_name = std::string(name) + " (Default View)";
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

  VkImageCreateInfo image_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = { extent.width, extent.height, 1 },
    .mipLevels = mip_levels,
    .arrayLayers = array_layers,
    .samples = static_cast<VkSampleCountFlagBits>(sample_count),
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  vmaCreateImage(device->get_allocator().get(),
                 &image_info,
                 &alloc_info,
                 &image,
                 &allocation,
                 nullptr);

  VkImageViewCreateInfo default_view_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .image = image,
    .viewType = array_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
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
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias = 0.0f,
    .anisotropyEnable = VK_FALSE,
    .maxAnisotropy = 1.f,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.f,
    .maxLod = VK_LOD_CLAMP_NONE,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
  };

  if (Attachment attachment{ format }; attachment.is_depth()) {
    sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.f,
      .compareEnable = VK_TRUE,
      .compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
      .minLod = 0.f,
      .maxLod = 0.f,
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

  for (auto view : mip_layer_views)
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

  device->flush(cmd, pool, device->graphics_queue());
}

auto
Image::load_from_file(const Device& device,
                      const std::string& path,
                      bool flip_y) -> std::unique_ptr<Image>
{
  stbi_set_flip_vertically_on_load(flip_y ? 1 : 0);

  int width, height, channels;
  stbi_uc* pixels =
    stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (!pixels) {
    std::cerr << "Failed to load image: " << path << "\n";
    return nullptr;
  }

  uint32_t mip_levels =
    1 + static_cast<uint32_t>(std::floor(std::log2(std::max(width, height))));

  const auto img_config = ImageConfiguration{
    .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) },
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .mip_levels = mip_levels,
    .array_layers = 1,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
    .sample_count = VK_SAMPLE_COUNT_1_BIT,
    .allow_in_ui = true,
    .debug_name = std::filesystem::path{ path }.filename().string(),
  };

  auto image = create(device, img_config);

  image->upload_rgba(
    std::span{ pixels, static_cast<size_t>(width * height * STBI_rgb_alpha) });

  stbi_image_free(pixels);

  return image;
}