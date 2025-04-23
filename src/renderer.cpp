#include "renderer.hpp"
#include "image_transition.hpp"
#include <functional>
#include <memory>

Renderer::Renderer(const Device& dev,
                   const BlueprintRegistry& registry,
                   const PipelineFactory& factory,
                   const ComputePipelineFactory& compute_factory,
                   const Window& win)
  : device(&dev)
  , blueprint_registry(&registry)
  , pipeline_factory(&factory)
  , compute_pipeline_factory(&compute_factory)
  , window(&win)
{
  command_buffer =
    CommandBuffer::create(dev, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  compute_command_buffer = std::make_unique<CommandBuffer>(
    dev,
    dev.compute_queue(),
    CommandBufferType::Compute,
    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  VkSemaphoreCreateInfo sem_info{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
  };
  for (auto& semaphore : compute_finished_semaphore) {
    vkCreateSemaphore(dev.get_device(), &sem_info, nullptr, &semaphore);
  }

  geometry_image = Image::create(dev,
                                 ImageConfiguration{
                                   .extent = win.framebuffer_size(),
                                   .format = VK_FORMAT_B8G8R8A8_UNORM,
                                 });

  geometry_pipeline =
    pipeline_factory->create_pipeline(blueprint_registry->get("main_geometry"));
  test_compute_pipeline = compute_pipeline_factory->create_pipeline(
    blueprint_registry->get("test_compute"));
}

Renderer::~Renderer()
{
  for (auto& semaphore : compute_finished_semaphore) {
    vkDestroySemaphore(device->get_device(), semaphore, nullptr);
  }
  command_buffer.reset();
  compute_command_buffer.reset();
  geometry_image.reset();
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

  run_compute_pass(frame_index);

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
  command_buffer->submit_and_end(frame_index,
                                 compute_finished_semaphore.at(frame_index));

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

auto
Renderer::run_compute_pass(std::uint32_t frame_index) -> void
{
  compute_command_buffer->begin_frame(frame_index);
  compute_command_buffer->begin_timer(frame_index, "test_compute_pass");

  const VkCommandBuffer cmd = compute_command_buffer->get(frame_index);
  (void)cmd;

  // Optional: insert debug marker or timestamp
  // vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, ...)

  /**
   * @brief #version 460

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void
main()
{
}
   *
   */
  vkCmdBindPipeline(
    cmd, test_compute_pipeline.bind_point, test_compute_pipeline.pipeline);
  vkCmdDispatch(cmd,
                1,  // x
                1,  // y
                1); // z

  compute_command_buffer->end_timer(frame_index, "test_compute_pass");

  compute_command_buffer->submit_and_end(
    frame_index,
    VK_NULL_HANDLE,
    compute_finished_semaphore.at(frame_index),
    VK_NULL_HANDLE);
}