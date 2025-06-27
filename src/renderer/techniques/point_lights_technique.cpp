#include "renderer/techniques/point_lights_technique.hpp"

#include "core/command_buffer.hpp"
#include "core/image.hpp"
#include "core/image_transition.hpp"
#include "core/vulkan_util.hpp"
#include "renderer/descriptor_manager.hpp"
#include "renderer/renderer.hpp"

#include <tracy/Tracy.hpp>

void
PointLightsTechnique::initialise(
  Renderer& renderer,
  const string_hash_map<const Image*>& values,
  const string_hash_map<const GPUBuffer*>& buffers)
{
  FullscreenTechniqueBase::initialise(renderer, values, buffers);

  const Image* first_image_for_extent = nullptr;
  bool any_resource_bound = false;

  for (const auto& [binding, source] : desc.inputs) {
    bool found = false;

    if (const auto image_it = values.find(source); image_it != values.end()) {
      material->upload(binding, image_it->second);
      if (!first_image_for_extent) {
        first_image_for_extent = image_it->second;
      }
      found = true;
    }

    if (const auto buffer_it = buffers.find(source);
        buffer_it != buffers.end()) {
      material->upload(binding, buffer_it->second);
      found = true;
    }

    if (found) {
      any_resource_bound = true;
    }
  }

  assert(any_resource_bound &&
         "No matching image or buffer resources found for any inputs");
  assert(
    first_image_for_extent &&
    "At least one image input is required to determine framebuffer extent");

  glm::uvec2 extent{};
  if (desc.output.extent == "framebuffer") {
    extent = first_image_for_extent->size();
  } else {
    assert(false && "Only 'framebuffer' extent output is currently supported");
  }

  output_image = Image::create(*device,
                               ImageConfiguration{
                                 .extent = extent,
                                 .format = desc.output.format,
                                 .usage = desc.output.usage,
                                 .debug_name = desc.output.name.c_str(),
                               });
}

auto
PointLightsTechnique::on_resize(const std::uint32_t width,
                                const std::uint32_t height) -> void
{
  output_image->resize(width, height);
  material->invalidate_all();
}

void
PointLightsTechnique::perform(const CommandBuffer& command_buffer,
                              const std::uint32_t frame_index) const
{
  ZoneScopedN("point_lights");

  const auto cmd = command_buffer.get(frame_index);
  command_buffer.begin_timer(frame_index, "point_lights");

  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Point Light GUI", { 1.0f, 1.0f, 0.0f, 1.0f });

  CoreUtils::cmd_transition_to_color_attachment(cmd, *output_image);

  VkRenderingAttachmentInfo attachment_info{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = output_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = { .color = { { 0.f, 0.f, 0.f, 0.f } } },
  };

  const VkRenderingInfo rendering_info{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { { 0, 0 },
                    { output_image->width(), output_image->height() } },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &attachment_info,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  const VkViewport viewport{
    .x = 0.f,
    .y = static_cast<float>(output_image->height()),
    .width = static_cast<float>(output_image->width()),
    .height = -static_cast<float>(output_image->height()),
    .minDepth = 0.f,
    .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto& pipeline = material->get_pipeline();
  vkCmdBindPipeline(cmd, desc.bind_point, pipeline.pipeline);

  const std::array descriptor_sets{
    descriptors->get_set(frame_index),
    material->prepare_for_rendering(frame_index),
  };

  vkCmdBindDescriptorSets(cmd,
                          desc.bind_point,
                          pipeline.layout,
                          0,
                          static_cast<std::uint32_t>(descriptor_sets.size()),
                          descriptor_sets.data(),
                          0,
                          nullptr);

  struct PointLightPushConstants
  {
    glm::vec2 screen_size;
  } constants{ glm::vec2(output_image->width(), output_image->height()) };

  vkCmdPushConstants(cmd,
                     pipeline.layout,
                     VK_SHADER_STAGE_FRAGMENT_BIT,
                     0,
                     sizeof(PointLightPushConstants),
                     &constants);

  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);

  CoreUtils::cmd_transition_to_shader_read(cmd, *output_image);
  command_buffer.end_timer(frame_index, "point_lights");

  Util::Vulkan::cmd_end_debug_label(cmd);
}
