#include "renderer/passes/bloom_pass.hpp"

#include "core/image.hpp"
#include "core/image_array.hpp"
#include "core/image_transition.hpp"
#include "core/vulkan_util.hpp"
#include "renderer/descriptor_manager.hpp"
#include "renderer/material.hpp"

#include <cassert>
#include <imgui.h>

auto
barrier(const Image& image, const auto cmd)
{
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // Previous write access
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;  // Next read access
  barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image.get_image();
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  vkCmdPipelineBarrier(
    cmd,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // From compute shader writing
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // To compute shader reading
    0,
    0,
    nullptr,
    0,
    nullptr,
    1,
    &barrier);
}

BloomPass::BloomPass(const Device& d, const Image* fb, int mips)
  : device(&d)
  , source_image(fb)
{
  assert(mips >= 3 && mips < 6);

  // Lets build an image array instead.
  image_array =
    ImageArray::create(*device,
                       ImageArray::create_mip_like(
                         glm::max(source_image->size() / 2u, glm::uvec2(1)),
                         mips,
                         0.5f,
                         "bloom_mip_array"));

  // Setup sampler config with CLAMP_TO_EDGE for all address modes
  SamplerConfiguration clamp_to_edge_sampler_config;
  clamp_to_edge_sampler_config.mag_filter = VK_FILTER_LINEAR;
  clamp_to_edge_sampler_config.min_filter = VK_FILTER_LINEAR;
  clamp_to_edge_sampler_config.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  clamp_to_edge_sampler_config.address_mode_u =
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  clamp_to_edge_sampler_config.address_mode_v =
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  clamp_to_edge_sampler_config.address_mode_w =
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  clamp_to_edge_sampler_config.border_color =
    VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  clamp_to_edge_sampler_config.unnormalized_coordinates = VK_FALSE;

  extract_image = Image::create(
    *device,
    {
      .extent = source_image->size(),
      .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initial_layout = VK_IMAGE_LAYOUT_GENERAL,
      .sampler_config = clamp_to_edge_sampler_config,
      .debug_name = "bloom_extract_image",
    });

  extract_material = Material::create(*device, "bloom_extract").value();
  extract_material->upload("input_image", source_image);
  extract_material->upload("output_image", extract_image.get());

  extract_material->upload("bloom_image_array", image_array);

  glm::uvec2 current_size = glm::max(source_image->size() / 2u, glm::uvec2(1));
  for (int i = 0; i < mips; ++i) {
    auto image = Image::create(
      *device,
      {
        .extent = current_size,
        .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initial_layout = VK_IMAGE_LAYOUT_GENERAL,
        .sampler_config = clamp_to_edge_sampler_config,
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

    auto blur_temp = Image::create(
      *device,
      {
        .extent = current_size,
        .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initial_layout = VK_IMAGE_LAYOUT_GENERAL,
        .sampler_config = clamp_to_edge_sampler_config,
        .debug_name = "blur_temp_" + std::to_string(i),
      });

    blur_temp_chain.push_back(std::move(blur_temp));

    current_size = glm::max(current_size / 2u, glm::uvec2(1));
    mip_chain.emplace_back(std::move(mip));
  }

  final_upsample_material =
    Material::create(*device, "bloom_final_upsample").value();
}

void
BloomPass::resize(const glm::uvec2& new_size)
{
  extract_image->resize(new_size.x, new_size.y);
  extract_material->invalidate(source_image);
  extract_material->invalidate(extract_image.get());
  extract_material->invalidate(image_array.get());

  glm::uvec2 current_size = glm::max(new_size / 2u, glm::uvec2(1));

  for (std::size_t i = 0; i < mip_chain.size(); ++i) {
    mip_chain[i].image->resize(current_size.x, current_size.y);
    mip_chain[i].downsample_material->invalidate(mip_chain[i].image.get());
    mip_chain[i].blur_horizontal->invalidate(mip_chain[i].image.get());
    mip_chain[i].blur_vertical->invalidate(mip_chain[i].image.get());
    mip_chain[i].upsample_material->invalidate(mip_chain[i].image.get());

    blur_temp_chain[i]->resize(current_size.x, current_size.y);

    current_size = glm::max(current_size / 2u, glm::uvec2(1));
  }

  final_upsample_material->invalidate(extract_image.get());
  final_upsample_material->invalidate(mip_chain[0].image.get());
}

void
BloomPass::update_source(const Image* image)
{
  source_image = image;
}

auto
BloomPass::on_interface() -> void
{
  ImGui::DragFloat("Bloom threshold", &config.threshold, 0.01f, 0.f, 10.f);
  ImGui::DragFloat("Knee", &config.knee, 0.01f, 0.f, 10.f);
}

auto
BloomPass::reload_pipeline(const PipelineBlueprint& blueprint,
                           const BloomPipeline pipeline_type) -> void
{
  const auto reload_and_invalidate = [&](auto& material) {
    material->reload(blueprint);
    material->invalidate_all();
  };

  if (pipeline_type == BloomPipeline::FinalUpsample) {
    reload_and_invalidate(final_upsample_material);
    return;
  }

  for (const auto& mip : mip_chain) {
    switch (pipeline_type) {
      case BloomPipeline::Horizontal: {
        reload_and_invalidate(mip.blur_horizontal);
        break;
      }
      case BloomPipeline::Vertical: {
        reload_and_invalidate(mip.blur_vertical);
        break;
      }
      case BloomPipeline::Downsample: {
        reload_and_invalidate(mip.downsample_material);
        break;
      }
      case BloomPipeline::Upsample: {
        reload_and_invalidate(mip.upsample_material);
        break;
      }
      default:
        break;
    }
  }
}

void
BloomPass::prepare(const uint32_t)
{
  // extract_material->invalidate(extract_image.get());
  // extract_material->invalidate(source_image);
}

auto
BloomPass::get_output_image() const -> const Image&
{
  return *extract_image;
}

auto
BloomPass::record(const VkCommandBuffer cmd,
                  DescriptorSetManager& dsm,
                  const std::uint32_t frame_index) -> void
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

  dispatch_compute_with_push_constant(
    cmd,
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
BloomPass::downsample_and_blur(const VkCommandBuffer cmd,
                               const DescriptorSetManager& dsm,
                               const uint32_t frame_index)
{
  Util::Vulkan::cmd_begin_debug_label(cmd,
                                      { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                                        nullptr,
                                        "Bloom Downsample & Blur",
                                        { 0.6f, 0.8f, 1.0f, 1.0f } });

  const Image* input_image = extract_image.get();

  for (std::size_t i = 0; i < mip_chain.size(); ++i) {
    auto& mip = mip_chain[i];
    auto& blur_temp = blur_temp_chain[i];

    const std::string mip_label = "Mip Level " + std::to_string(i);
    Util::Vulkan::cmd_begin_debug_label(
      cmd,
      { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        nullptr,
        mip_label.c_str(),
        { 0.3f, 0.5f, 1.0f, 1.0f } });

    Util::Vulkan::cmd_begin_debug_label(
      cmd,
      { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        nullptr,
        "Downsample",
        { 0.7f, 0.3f, 0.8f, 1.0f } });

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
    // TODO: Should this be input_image or mip.image?
    barrier(*mip.image, cmd);

    Util::Vulkan::cmd_end_debug_label(cmd); // Downsample

    Util::Vulkan::cmd_begin_debug_label(
      cmd,
      { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        nullptr,
        "Blur Horizontal",
        { 0.9f, 0.6f, 0.2f, 1.0f } });

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

    Util::Vulkan::cmd_end_debug_label(cmd); // Blur Horizontal

    // TODO: Should this be blur_temp or mip.image?
    barrier(*mip.image, cmd);

    Util::Vulkan::cmd_begin_debug_label(
      cmd,
      { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        nullptr,
        "Blur Vertical",
        { 0.8f, 0.4f, 0.1f, 1.0f } });

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
    // TODO: Should this be blur_temp or mip.image?
    barrier(*mip.image, cmd);

    Util::Vulkan::cmd_end_debug_label(cmd); // Blur Vertical

    Util::Vulkan::cmd_end_debug_label(cmd); // Mip Level
    input_image = mip.image.get();
  }

  Util::Vulkan::cmd_end_debug_label(cmd); // Downsample & Blur
}

void
BloomPass::upsample_and_combine(const VkCommandBuffer cmd,
                                const DescriptorSetManager& dsm,
                                const std::uint32_t frame_index)
{
  Util::Vulkan::cmd_begin_debug_label(cmd,
                                      { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                                        nullptr,
                                        "Bloom Upsample",
                                        { 0.4f, 1.0f, 0.6f, 1.0f } });

  for (int i = static_cast<int>(mip_chain.size()) - 1; i >= 0; --i) {
    if (i == 0) {
      // Final upsample: mip 0 -> extract_image
      auto& current_mip = mip_chain[i];

      const std::string label = "Final Upsample to Extract";
      Util::Vulkan::cmd_begin_debug_label(
        cmd, label, { 0.6f, 1.0f, 0.4f, 1.0f });

      final_upsample_material->upload("input_image", current_mip.image.get());
      final_upsample_material->upload("output_image", extract_image.get());
      final_upsample_material->invalidate(current_mip.image.get());
      final_upsample_material->invalidate(extract_image.get());

      dispatch_compute(
        cmd,
        *final_upsample_material,
        std::array{
          dsm.get_set(frame_index),
          final_upsample_material->prepare_for_rendering(frame_index),
        },
        { extract_image->width(), extract_image->height() });

      Util::Vulkan::cmd_end_debug_label(cmd); // Final Upsample
    } else {
      // Normal case: mip i -> mip i-1
      auto& lower = mip_chain[i];
      auto& higher = mip_chain[i - 1];

      const std::string label = "Upsample Mip " + std::to_string(i - 1);
      Util::Vulkan::cmd_begin_debug_label(
        cmd,
        { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
          nullptr,
          label.c_str(),
          { 0.6f, 1.0f, 0.4f, 1.0f } });

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

      Util::Vulkan::cmd_end_debug_label(cmd); // Upsample Mip N
    }
  }

  Util::Vulkan::cmd_end_debug_label(cmd); // Bloom Upsample
}

void
BloomPass::dispatch_compute(const VkCommandBuffer cmd,
                            const Material& material,
                            const std::span<const VkDescriptorSet> sets,
                            const glm::uvec2 extent)
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

  static constexpr auto ceil_div = [](const std::uint32_t x) {
    return (x + 8 - 1) / 8;
  };
  auto groups = glm::uvec3(ceil_div(extent.x), ceil_div(extent.y), 1);
  vkCmdDispatch(cmd, groups.x, groups.y, groups.z);
}

void
BloomPass::dispatch_compute_with_push_constant(
  const VkCommandBuffer cmd,
  const Material& material,
  const std::span<const VkDescriptorSet> sets,
  const glm::uvec2 extent)
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

  static constexpr auto ceil_div = [](const std::uint32_t x) {
    return (x + 8 - 1) / 8;
  };
  auto groups = glm::uvec3(ceil_div(extent.x), ceil_div(extent.y), 1);
  vkCmdPushConstants(cmd,
                     pipeline.layout,
                     VK_SHADER_STAGE_COMPUTE_BIT,
                     0,
                     sizeof(BloomConfig),
                     &config);
  vkCmdDispatch(cmd, groups.x, groups.y, groups.z);
}
