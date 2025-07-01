#pragma once

#include "assets/pointer.hpp"
#include "core/image.hpp"
#include "core/image_configuration.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class ImageArray
{
private:
  Assets::Pointer<Image> image;

public:
  explicit ImageArray(Assets::Pointer<Image> img)
    : image(std::move(img))
  {
  }
  ~ImageArray();

  auto get_image() const -> Image* { return image.get(); }

  struct CreateInfo
  {
    std::vector<glm::uvec2> layer_extents; // Size of each layer
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkImageUsageFlags usage =
      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageLayout initial_layout = VK_IMAGE_LAYOUT_GENERAL;
    SamplerConfiguration sampler_config = {};
    std::string debug_name = "image_array";
  };

  static auto create(const Device& d, const ImageArray::CreateInfo& info)
    -> Assets::Pointer<ImageArray>;

  static auto create_mip_like(glm::uvec2 base_extent,
                              int layer_count,
                              float scale_factor = 0.5f,
                              const std::string& debug_name = "mip_array")
    -> ImageArray::CreateInfo
  {

    CreateInfo info;
    info.debug_name = debug_name;
    info.layer_extents.reserve(layer_count);

    glm::uvec2 current_size = base_extent;
    for (int i = 0; i < layer_count; ++i) {
      info.layer_extents.push_back(current_size);
      current_size = glm::max(glm::uvec2(current_size.x * scale_factor,
                                         current_size.y * scale_factor),
                              glm::uvec2(1));
    }
    return info;
  }
};