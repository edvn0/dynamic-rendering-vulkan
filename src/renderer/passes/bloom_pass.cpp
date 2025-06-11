#include "renderer/passes/bloom_pass.hpp"

#include "core/image.hpp"
#include "core/image_transition.hpp"
#include "core/vulkan_util.hpp"
#include "renderer/descriptor_manager.hpp"
#include "renderer/material.hpp"

#include <cassert>

BloomPass::BloomPass(const Device& device, const glm::uvec2& size, int)
  : device(&device)
{
  extract_image = Image::create(
    device,
    { .extent = size,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .debug_name = "bloom_extract_image" });

  extract_material = Material::create(device, "bloom_extract").value();
  // downsample_material = Material::create(device, "bloom_downsample").value();

  mip_chain.clear();
  mip_chain.reserve(5);
  /*
    glm::uvec2 current_size = size;
    for (int i = 0; i < 5; ++i) {
      current_size = glm::max(current_size / 2u, glm::uvec2(1));

      BloomMip mip = {
        .image = Image::create(
          device,
          {
            .extent = current_size,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .debug_name = "bloom_mip_" + std::to_string(i),
          }),
        .blur_horizontal = Material::create(device, "bloom_horizontal").value(),
        .blur_vertical = Material::create(device, "bloom_vertical").value(),
      };

      mip_chain.emplace_back(std::move(mip));
    }
    */
}

void
BloomPass::resize(const glm::uvec2& size)
{
  extract_image->resize(size.x, size.y);
  extract_material->upload("input_image", source_image);
  extract_material->upload("output_image", extract_image.get());
}

void
BloomPass::update_source(const Image* image)
{
  source_image = image;
  extract_material->upload("input_image", source_image);
  extract_material->upload("output_image", extract_image.get());
}

void
BloomPass::prepare(const uint32_t frame_index)
{
  extract_material->prepare_for_rendering(frame_index);

  const Image* input = extract_image.get();
  for (auto& mip : mip_chain) {
    downsample_material->upload("input_image", input);
    downsample_material->prepare_for_rendering(frame_index);
    input = mip.image.get();
  }
}

auto
BloomPass::get_output_image() const -> const Image&
{
  return *extract_image;
}

void
BloomPass::record(VkCommandBuffer cmd,
                  DescriptorSetManager& dsm,
                  uint32_t frame_index)
{
  assert(source_image != nullptr);

  constexpr VkDebugUtilsLabelEXT bloom_label = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
    .pLabelName = "Bloom Pass",
    .color = { 1.0f, 0.5f, 0.0f, 1.0f }
  };
  Util::Vulkan::cmd_begin_debug_label(cmd, bloom_label);

  constexpr VkDebugUtilsLabelEXT extract_label = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
    .pLabelName = "Bloom Extract",
    .color = { 1.0f, 1.0f, 0.0f, 1.0f }
  };
  Util::Vulkan::cmd_begin_debug_label(cmd, extract_label);

  CoreUtils::cmd_transition_to_general(cmd, *extract_image);
  dispatch_compute(cmd,
                   *extract_material,
                   dsm,
                   frame_index,
                   { extract_image->width(), extract_image->height() });
  CoreUtils::cmd_transition_to_shader_read(cmd, *extract_image);

  Util::Vulkan::cmd_end_debug_label(cmd);
  Util::Vulkan::cmd_end_debug_label(cmd);
}

auto
BloomPass::downsample(const VkCommandBuffer,
                      DescriptorSetManager&,
                      const uint32_t) -> void
{
  // No-op for now.
}

void
BloomPass::dispatch_compute(VkCommandBuffer cmd,
                            Material& material,
                            DescriptorSetManager& dsm,
                            uint32_t frame_index,
                            glm::uvec2 extent)
{
  const auto& pipeline = material.get_pipeline();
  const auto& set = material.prepare_for_rendering(frame_index);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);

  const std::array sets{ dsm.get_set(frame_index), set };
  vkCmdBindDescriptorSets(cmd,
                          pipeline.bind_point,
                          pipeline.layout,
                          0,
                          static_cast<uint32_t>(sets.size()),
                          sets.data(),
                          0,
                          nullptr);

  struct PushConstantData
  {
    glm::vec2 framebuffer_size;
  };
  const PushConstantData pc = {
    .framebuffer_size = glm::vec2(extent),
  };

  vkCmdPushConstants(cmd,
                     pipeline.layout,
                     VK_SHADER_STAGE_COMPUTE_BIT,
                     0,
                     sizeof(PushConstantData),
                     &pc);

  vkCmdDispatch(cmd, (extent.x + 7) / 8, (extent.y + 7) / 8, 1);
}
