#include "renderer/techniques/shadow_gui_technique.hpp"

#include "core/command_buffer.hpp"
#include "core/image.hpp"
#include "core/image_transition.hpp"
#include "core/vulkan_util.hpp"
#include "renderer/descriptor_manager.hpp"
#include "renderer/renderer.hpp"

void
ShadowGUITechnique::initialise(Renderer& renderer,
                               const string_hash_map<const Image*>& values,
                               const string_hash_map<const GPUBuffer*>& pairs)
{
  FullscreenTechniqueBase::initialise(renderer, values, pairs);

  auto&& [binding, source] = desc.inputs.at(0);
  auto& image = values.at(source);

  glm::uvec2 extent{};
  assert(desc.output.extent == "framebuffer" &&
         "Needs to be framebuffer for now");
  if (desc.output.extent == "framebuffer") {
    extent = image->size();
  }

  output_image = Image::create(*device,
                               ImageConfiguration{
                                 .extent = extent,
                                 .format = desc.output.format,
                                 .usage = desc.output.usage,
                                 .debug_name = desc.output.name.c_str(),
                               });

  renderer_camera_environment = &renderer.get_camera_environment();

  material->upload(binding, image);
}

void
ShadowGUITechnique::perform(const CommandBuffer& command_buffer,
                            const std::uint32_t frame_index) const
{
  const auto cmd = command_buffer.get(frame_index);

  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Shadow GUI", { 0.1, 1.0, 1.0, 1.0 });

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
                    {
                      output_image->width(),
                      output_image->height(),
                    }, },
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

  const struct ShadowGUIPushConstants
  {
    float z_near{};
    float z_far{};
  } data{ renderer_camera_environment->z_near,
          renderer_camera_environment->z_far };

  vkCmdPushConstants(cmd,
                     pipeline.layout,
                     VK_SHADER_STAGE_FRAGMENT_BIT,
                     0,
                     sizeof(ShadowGUIPushConstants),
                     &data);

  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);

  CoreUtils::cmd_transition_to_shader_read(cmd, *output_image);

  Util::Vulkan::cmd_end_debug_label(cmd);
}
