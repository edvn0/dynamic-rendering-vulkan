#pragma once

#include "config.hpp"
#include "device.hpp"
#include "image_configuration.hpp"

#include "allocator.hpp"

#include <memory>
#include <utility>

class Image
{
public:
  static auto create(const Device& device, const ImageConfiguration& config)
    -> std::unique_ptr<Image>
  {
    VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = config.format,
        .extent =
            {
                config.extent.width,
                config.extent.height,
                1,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage image{};
    VmaAllocation allocation{};
    vmaCreateImage(device.get_allocator().get(),
                   &image_info,
                   &alloc_info,
                   &image,
                   &allocation,
                   nullptr);

    VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = config.format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkImageView view{};
    vkCreateImageView(device.get_device(), &view_info, nullptr, &view);

    return std::unique_ptr<Image>(
      new Image(image, view, allocation, config.extent, device));
  }

  ~Image()
  {
    vkDestroyImageView(device->get_device(), view, nullptr);
    vmaDestroyImage(device->get_allocator().get(), image, allocation);
  }

  auto get_view() const -> VkImageView { return view; }
  auto width() const -> uint32_t { return extent.width; }
  auto height() const -> uint32_t { return extent.height; }

private:
  Image(VkImage img,
        VkImageView v,
        VmaAllocation alloc,
        Extent2D ext,
        const Device& dev)
    : image(img)
    , view(v)
    , allocation(alloc)
    , extent(ext)
    , device(&dev)
  {
  }

  VkImage image{};
  VkImageView view{};
  VmaAllocation allocation{};
  Extent2D extent{};
  const Device* device;
};
