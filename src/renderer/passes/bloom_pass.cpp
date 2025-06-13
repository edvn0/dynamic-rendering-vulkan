#include "renderer/passes/bloom_pass.hpp"

#include "core/image.hpp"
#include "core/image_transition.hpp"
#include "core/vulkan_util.hpp"
#include "renderer/descriptor_manager.hpp"
#include "renderer/material.hpp"

#include <cassert>

BloomPass::BloomPass(const Device& d, const glm::uvec2& size, int mips)
  : device(&d)
{
  assert(mips >= 3 && mips < 6);

  extract_image = Image::create(
    *device,
    {
      .extent = size,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initial_layout = VK_IMAGE_LAYOUT_GENERAL,
      .sampler_config = SamplerConfiguration{},
      .debug_name = "bloom_extract_image",
    });

  extract_material = Material::create(*device, "bloom_extract").value();

  glm::uvec2 current_size = glm::max(size / 2u, glm::uvec2(1));
  for (int i = 0; i < mips; ++i) {
    auto image = Image::create(
      *device,
      {
        .extent = current_size,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initial_layout = VK_IMAGE_LAYOUT_GENERAL,
        .sampler_config = SamplerConfiguration{},
        .debug_name = "bloom_mip_" + std::to_string(i),
      });

    BloomMip mip = {
      .image = std::move(image),
      .blur_horizontal = Material::create(*device, "bloom_horizontal").value(),
      .blur_vertical = Material::create(*device, "bloom_vertical").value(),
      .downsample_material =
        Material::create(*device, "bloom_downsample").value(),
      .upsample_material = Material::create(*device, "bloom_upsample").value(),
    };

    current_size = glm::max(current_size / 2u, glm::uvec2(1));
    mip_chain.emplace_back(std::move(mip));
  }

  blur_temp = Image::create(
    *device,
    {
      .extent = current_size,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initial_layout = VK_IMAGE_LAYOUT_GENERAL,
      .sampler_config = SamplerConfiguration{},
      .debug_name = "blur_temp",
    });
}

void
BloomPass::resize(const glm::uvec2& size)
{
  extract_image->resize(size.x, size.y);
  extract_material->upload("input_image", source_image);
  extract_material->upload("output_image", extract_image.get());
  extract_material->invalidate(extract_image.get());
  extract_material->invalidate(source_image);
}

void
BloomPass::update_source(const Image* image)
{
  source_image = image;
  extract_material->upload("input_image", source_image);
  extract_material->upload("output_image", extract_image.get());
}

void
BloomPass::prepare(const uint32_t)
{
  extract_material->upload("input_image", source_image);
  extract_material->upload("output_image", extract_image.get());
  extract_material->invalidate(extract_image.get());
  extract_material->invalidate(source_image);
}

auto
BloomPass::get_output_image() const -> const Image&
{
  return *mip_chain.front().image;
}

void
BloomPass::record(VkCommandBuffer cmd,
                  DescriptorSetManager& dsm,
                  uint32_t frame_index)
{
  assert(source_image != nullptr);

  Util::Vulkan::cmd_begin_debug_label(cmd,
                                      { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                                        nullptr,
                                        "Bloom Pass",
                                        { 1.0f, 0.5f, 0.0f, 1.0f } });
  Util::Vulkan::cmd_begin_debug_label(cmd,
                                      { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                                        nullptr,
                                        "Bloom Extract",
                                        { 1.0f, 1.0f, 0.0f, 1.0f } });

  dispatch_compute(cmd,
                   *extract_material,
                   std::array{
                     dsm.get_set(frame_index),
                     extract_material->prepare_for_rendering(frame_index),
                   },
                   { extract_image->width(), extract_image->height() });

  Util::Vulkan::cmd_end_debug_label(cmd);

  downsample_and_blur(cmd, dsm, frame_index);
  upsample_and_combine(cmd, dsm, frame_index);

  Util::Vulkan::cmd_end_debug_label(cmd);
}

void
BloomPass::downsample_and_blur(VkCommandBuffer cmd,
                               DescriptorSetManager& dsm,
                               uint32_t frame_index)
{
  const Image* input_image = extract_image.get();

  for (size_t i = 0; i < mip_chain.size(); ++i) {
    auto& mip = mip_chain[i];
    mip.downsample_material->upload("input_image", input_image);
    mip.downsample_material->upload("output_image", mip.image.get());
    mip.downsample_material->invalidate(input_image);
    mip.downsample_material->invalidate(mip.image.get());

    dispatch_compute(
      cmd,
      *mip.downsample_material,
      std::array{
        dsm.get_set(frame_index),
        mip.downsample_material->prepare_for_rendering(frame_index),
      },
      { mip.image->width(), mip.image->height() });

    mip.blur_horizontal->upload("input_image", mip.image.get());
    mip.blur_horizontal->upload("output_image", blur_temp.get());
    mip.blur_horizontal->invalidate(mip.image.get());
    mip.blur_horizontal->invalidate(blur_temp.get());

    dispatch_compute(cmd,
                     *mip.blur_horizontal,
                     std::array{
                       dsm.get_set(frame_index),
                       mip.blur_horizontal->prepare_for_rendering(frame_index),
                     },
                     { mip.image->width(), mip.image->height() });

    mip.blur_vertical->upload("input_image", blur_temp.get());
    mip.blur_vertical->upload("output_image", mip.image.get());
    mip.blur_vertical->invalidate(blur_temp.get());
    mip.blur_vertical->invalidate(mip.image.get());

    dispatch_compute(cmd,
                     *mip.blur_vertical,
                     std::array{
                       dsm.get_set(frame_index),
                       mip.blur_vertical->prepare_for_rendering(frame_index),
                     },
                     { mip.image->width(), mip.image->height() });

    input_image = mip.image.get();
  }
}

void
BloomPass::upsample_and_combine(VkCommandBuffer cmd,
                                DescriptorSetManager& dsm,
                                uint32_t frame_index)
{
  for (int i = static_cast<int>(mip_chain.size()) - 1; i > 0; --i) {
    auto& lower = mip_chain[i];
    auto& higher = mip_chain[i - 1];

    higher.upsample_material->upload("input_image", lower.image.get());
    higher.upsample_material->upload("output_image", higher.image.get());
    higher.upsample_material->invalidate(lower.image.get());
    higher.upsample_material->invalidate(higher.image.get());

    dispatch_compute(
      cmd,
      *higher.upsample_material,
      std::array{
        dsm.get_set(frame_index),
        higher.upsample_material->prepare_for_rendering(frame_index),
      },
      { higher.image->width(), higher.image->height() });
  }
}

void
BloomPass::dispatch_compute(VkCommandBuffer cmd,
                            Material& material,
                            const std::span<const VkDescriptorSet> sets,
                            glm::uvec2 extent)
{
  const auto& pipeline = material.get_pipeline();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);
  vkCmdBindDescriptorSets(cmd,
                          pipeline.bind_point,
                          pipeline.layout,
                          0,
                          static_cast<uint32_t>(sets.size()),
                          sets.data(),
                          0,
                          nullptr);

  const auto ceil_div = [](uint32_t x, uint32_t y) { return (x + y - 1) / y; };
  vkCmdDispatch(cmd, ceil_div(extent.x, 8), ceil_div(extent.y, 8), 1);
}
