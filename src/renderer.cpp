#include "renderer.hpp"
#include "image_transition.hpp"
#include <functional>
#include <memory>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

struct CameraBuffer
{
  glm::mat4 vp;
};

auto
Renderer::create_descriptor_set_layout() -> void
{
  VkDescriptorSetLayoutBinding binding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr,
  };
  VkDescriptorSetLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = 1,
    .pBindings = &binding,
  };
  vkCreateDescriptorSetLayout(device->get_device(),
                              &layout_info,
                              nullptr,
                              &renderer_descriptor_set_layout);

  std::array<VkDescriptorPoolSize, 1> pool_sizes{ {
    {
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = image_count,
    },
  } };

  VkDescriptorPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .maxSets = image_count,
    .poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  };

  vkCreateDescriptorPool(
    device->get_device(), &pool_info, nullptr, &descriptor_pool);

  const std::array layouts{ renderer_descriptor_set_layout,
                            renderer_descriptor_set_layout,
                            renderer_descriptor_set_layout };
  VkDescriptorSetAllocateInfo alloc_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = descriptor_pool,
    .descriptorSetCount = image_count,
    .pSetLayouts = layouts.data(),
  };

  vkAllocateDescriptorSets(
    device->get_device(), &alloc_info, renderer_descriptor_sets.data());

  {
    camera_uniform_buffer = std::make_unique<GPUBuffer>(
      *device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
    const auto camera_buffer_bytes =
      std::make_unique<std::byte[]>(sizeof(CameraBuffer) * image_count);
    camera_uniform_buffer->upload(std::span<std::byte>{
      camera_buffer_bytes.get(),
      sizeof(CameraBuffer) * image_count,
    });
  }

  for (std::size_t i = 0; i < image_count; ++i) {
    VkDescriptorBufferInfo buffer_info{
      .buffer = camera_uniform_buffer->get(),
      .offset = sizeof(CameraBuffer) * i,
      .range = sizeof(CameraBuffer),
    };

    VkWriteDescriptorSet write{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = renderer_descriptor_sets[i],
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pImageInfo = nullptr,
      .pBufferInfo = &buffer_info,
      .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(device->get_device(), 1, &write, 0, nullptr);
  }
}

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
  create_descriptor_set_layout();

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
  geometry_depth_image =
    Image::create(dev,
                  ImageConfiguration{
                    .extent = win.framebuffer_size(),
                    .format = VK_FORMAT_D32_SFLOAT,
                    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                  });

  geometry_pipeline = pipeline_factory->create_pipeline(
    blueprint_registry->get("main_geometry"),
    {
      .renderer_set_layout = renderer_descriptor_set_layout,
    });
  test_compute_pipeline = compute_pipeline_factory->create_pipeline(
    blueprint_registry->get("test_compute"));

  {
    instance_vertex_buffer =
      std::make_unique<GPUBuffer>(dev, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true);
    // Size this to 48bytes (3 glm::vec4s) * 10000 instances
    static constexpr auto instance_size = sizeof(glm::vec4) * 3;
    static constexpr auto instance_count = 1'000'000;
    static constexpr auto instance_size_bytes = instance_size * instance_count;
    const auto bytes = std::make_unique<std::byte[]>(instance_size_bytes);
    instance_vertex_buffer->upload(
      std::span<std::byte>{ bytes.get(), instance_size_bytes });
  }
}

auto
Renderer::destroy() -> void
{
  for (auto& semaphore : compute_finished_semaphore) {
    vkDestroySemaphore(device->get_device(), semaphore, nullptr);
  }
  vkDestroyDescriptorPool(device->get_device(), descriptor_pool, nullptr);
  vkDestroyDescriptorSetLayout(
    device->get_device(), renderer_descriptor_set_layout, nullptr);

  test_compute_pipeline.reset();
  geometry_pipeline.reset();

  camera_uniform_buffer.reset();
  geometry_depth_image.reset();
  geometry_image.reset();
  default_geometry_material.reset();
  compute_command_buffer.reset();
  instance_vertex_buffer.reset();
  command_buffer.reset();

  destroyed = true;
}

Renderer::~Renderer()
{
  if (!destroyed) {
    destroy();
  }
}

auto
Renderer::submit(const DrawCommand& cmd, const glm::mat4& transform) -> void
{
  const glm::vec3 center_ws = glm::vec3(transform[3]);
  const float radius_ws = 1.0f * glm::length(glm::vec3(transform[0]));

  if (!current_frustum.intersects(center_ws, radius_ws))
    return;

  glm::quat rotation = glm::quat_cast(transform);
  glm::vec4 rotation_vec =
    glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w);

  glm::vec3 translation = glm::vec3(transform[3]);
  glm::vec3 scale(glm::length(glm::vec3(transform[0])),
                  glm::length(glm::vec3(transform[1])),
                  glm::length(glm::vec3(transform[2])));

  glm::vec4 translation_and_scale(translation, 1.0f);
  glm::vec4 non_uniform_scale(scale, 0.0f);

  draw_commands[cmd].emplace_back(
    rotation_vec, translation_and_scale, non_uniform_scale);
}

static auto
upload_instance_vertex_data(GPUBuffer& buffer, const auto& draw_commands)
  -> std::vector<std::pair<DrawCommand, std::uint32_t>>
{
  std::vector<InstanceData> flattened_instances;
  std::vector<std::pair<DrawCommand, std::uint32_t>> flat_draw_commands;

  for (const auto& [cmd, instances] : draw_commands) {
    flat_draw_commands.emplace_back(
      cmd, static_cast<std::uint32_t>(flattened_instances.size()));
    flattened_instances.insert(
      flattened_instances.end(), instances.begin(), instances.end());
  }

  buffer.upload(
    std::span{ flattened_instances.data(), flattened_instances.size() });

  return flat_draw_commands;
}

auto
Renderer::update_uniform_buffers(const glm::mat4& vp) -> void
{
  static CameraBuffer buffer{ vp };
  const std::array buffers{ buffer, buffer, buffer };
  camera_uniform_buffer->upload(std::span(buffers));
}

auto
Renderer::end_frame(std::uint32_t frame_index,
                    const glm::mat4& projection,
                    const glm::mat4& view) -> void
{

  if (draw_commands.empty())
    return;

  const auto view_projection = projection * view;
  update_uniform_buffers(view_projection);

  run_compute_pass(frame_index);

  auto flat_draw_commands =
    upload_instance_vertex_data(*instance_vertex_buffer, draw_commands);

  command_buffer->begin_frame(frame_index);
  command_buffer->begin_timer(frame_index, "geometry_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_to_color_attachment(cmd,
                                                geometry_image->get_image());
  CoreUtils::cmd_transition_to_depth_attachment(
    cmd, geometry_depth_image->get_image());

  VkClearValue clear_value = { .color = { { 0.f, 0.f, 0.f, 0.f } } };
  VkRenderingAttachmentInfo color_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = clear_value
  };

  // Reverse the depth clear value to 1.0f for depth testing
  VkClearValue depth_clear_value = { .depthStencil = { 0.0f, 0 } };

  VkRenderingAttachmentInfo depth_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_depth_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .clearValue = depth_clear_value
  };

  VkRenderingInfo render_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { .offset = { 0, 0 },
                    .extent = { geometry_image->width(),
                                geometry_image->height(), }, },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
    .pDepthAttachment = &depth_attachment
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

  for (const auto& [command, offset] : flat_draw_commands) {
    const auto& geometry_material = command.override_material
                                      ? command.override_material
                                      : default_geometry_material.get();
    (void)geometry_material;

    const auto instance_count =
      static_cast<std::uint32_t>(draw_commands[command].size());

    const std::array vertex_buffers = {
      command.vertex_buffer->get(),
      instance_vertex_buffer->get(),
    };
    constexpr std::array<VkDeviceSize, 2> offsets = {
      0ULL,
      0ULL,
    };
    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(cmd,
                         command.index_buffer->get(),
                         0,
                         command.index_buffer->get_index_type());
    vkCmdBindPipeline(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, geometry_pipeline->pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            geometry_pipeline->layout,
                            0,
                            1,
                            &renderer_descriptor_sets[frame_index],
                            0,
                            nullptr);
    vkCmdDrawIndexed(
      cmd,
      static_cast<std::uint32_t>(command.index_buffer->get_count()),
      instance_count,
      0,
      0,
      offset);
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
  geometry_depth_image->resize(width, height);
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

  vkCmdBindPipeline(
    cmd, test_compute_pipeline->bind_point, test_compute_pipeline->pipeline);
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