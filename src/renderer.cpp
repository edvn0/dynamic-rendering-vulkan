#include "renderer.hpp"
#include "image_transition.hpp"
#include <functional>
#include <memory>

Renderer::Renderer(const Device& dev,
                   const BlueprintRegistry& registry,
                   const PipelineFactory& factory,
                   const Window& win)
  : device(&dev)
  , blueprint_registry(&registry)
  , pipeline_factory(&factory)
  , window(&win)
{
  command_buffer =
    CommandBuffer::create(dev, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  compute_command_buffer = std::make_unique<CommandBuffer>(
    dev, dev.compute_queue(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  geometry_image = Image::create(dev,
                                 ImageConfiguration{
                                   .extent = win.framebuffer_size(),
                                   .format = VK_FORMAT_B8G8R8A8_UNORM,
                                 });

  geometry_pipeline =
    pipeline_factory->create_pipeline(blueprint_registry->get("main_geometry"));
}

auto
Renderer::submit(const DrawCommand& command) -> void
{
  draw_commands[command]++;
}

auto
Renderer::end_frame(std::uint32_t frame_index) -> void
{
  if (draw_commands.empty())
    return;

  command_buffer->begin_frame(frame_index);
  command_buffer->begin_timer(frame_index, "geometry_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_to_color_attachment(cmd,
                                                geometry_image->get_image());

  VkClearValue clear_value = { .color = { { 0.f, 0.f, 0.f, 0.f } } };
  VkRenderingAttachmentInfo color_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = geometry_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = clear_value
  };

  VkRenderingInfo render_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { .offset = { 0, 0 },
                    .extent = { geometry_image->width(),
                                geometry_image->height() } },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment
  };

  vkCmdBeginRendering(cmd, &render_info);

  VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(geometry_image->height()),
    .width = static_cast<float>(geometry_image->width()),
    .height = -static_cast<float>(geometry_image->height()),
    .minDepth = 0.f,
    .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &render_info.renderArea);

  for (const auto& [draw, count] : draw_commands) {
    /* Material* material = draw.override_material
                           ? draw.override_material
                           : default_geometry_material.get();
 */
    vkCmdBindPipeline(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, geometry_pipeline.pipeline);

    const VkBuffer vertex_buffers[] = { draw.vertex_buffer->get() };
    const VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(
      cmd, draw.index_buffer->get(), 0, draw.index_buffer->get_index_type());
    vkCmdDrawIndexed(cmd,
                     static_cast<std::uint32_t>(draw.index_buffer->get_count()),
                     count,
                     0,
                     0,
                     0);
  }

  vkCmdEndRendering(cmd);
  CoreUtils::cmd_transition_to_shader_read(cmd, geometry_image->get_image());

  command_buffer->end_timer(frame_index, "geometry_pass");
  command_buffer->submit_and_end(frame_index);

  draw_commands.clear();
}

auto
Renderer::resize(std::uint32_t width, std::uint32_t height) -> void
{
  geometry_image->resize(width, height);
}

auto
Renderer::get_output_image() const -> const Image&
{
  return *geometry_image;
}
