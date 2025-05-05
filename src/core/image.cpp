#include "core/image.hpp"

#include "pipeline/blueprint_configuration.hpp"

#include "imgui_impl_vulkan.h"

#include "core/allocator.hpp"

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
      .compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
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
