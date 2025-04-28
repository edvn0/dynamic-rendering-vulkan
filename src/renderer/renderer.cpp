#include "renderer/renderer.hpp"

#include "core/image_transition.hpp"

#include <execution>
#include <functional>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <vulkan/vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

struct FrustumBuffer
{
  std::array<glm::vec4, 6> planes{};
  std::array<glm::vec4, 2> _padding_{};
};

struct CameraBuffer
{
  const glm::mat4 vp;
  const glm::mat4 inverse_vp;
};

struct ShadowBuffer
{
  glm::mat4 light_vp;
  glm::vec4 light_position;
  glm::vec4 light_color;
  std::array<glm::vec4, 2> padding{};
};

template<typename T, std::size_t N = image_count>
static constexpr auto
create_sized_array(const T& value) -> std::array<T, N>
{
  std::array<T, N> arr{};
  arr.fill(value);
  return arr;
}

auto
Renderer::create_descriptor_set_layout() -> void
{
  VkDescriptorSetLayoutBinding binding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                  VK_SHADER_STAGE_COMPUTE_BIT,
    .pImmutableSamplers = nullptr,
  };
  VkDescriptorSetLayoutBinding shadow_binding{
    .binding = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr,
  };
  VkDescriptorSetLayoutBinding frustum_binding{
    .binding = 2,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                  VK_SHADER_STAGE_COMPUTE_BIT,
    .pImmutableSamplers = nullptr,
  };
  const std::array bindings{ binding, shadow_binding, frustum_binding };
  VkDescriptorSetLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = static_cast<std::uint32_t>(bindings.size()),
    .pBindings = bindings.data(),
  };
  vkCreateDescriptorSetLayout(device->get_device(),
                              &layout_info,
                              nullptr,
                              &renderer_descriptor_set_layout);

  std::array<VkDescriptorPoolSize, bindings.size()> pool_sizes{
    { {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = image_count,
      },
      {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = image_count,
      },
      {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = image_count,
      } }
  };

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

  const auto layouts = create_sized_array(renderer_descriptor_set_layout);
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
  {
    shadow_camera_buffer = std::make_unique<GPUBuffer>(
      *device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
    const auto shadow_bytes =
      std::make_unique<std::byte[]>(sizeof(ShadowBuffer) * image_count);
    shadow_camera_buffer->upload(std::span<std::byte>{
      shadow_bytes.get(),
      sizeof(ShadowBuffer) * image_count,
    });
  }
  {
    frustum_buffer = std::make_unique<GPUBuffer>(
      *device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
    const auto frustum_bytes =
      std::make_unique<std::byte[]>(sizeof(FrustumBuffer) * image_count);
    frustum_buffer->upload(std::span<std::byte>{
      frustum_bytes.get(),
      sizeof(FrustumBuffer) * image_count,
    });
  }

  std::vector<VkDescriptorBufferInfo> buffer_infos;
  buffer_infos.resize(bindings.size() * image_count);

  for (std::size_t i = 0; i < image_count; ++i) {
    VkDescriptorBufferInfo& camera_info = buffer_infos[i * 3 + 0];
    camera_info = {
      .buffer = camera_uniform_buffer->get(),
      .offset = sizeof(CameraBuffer) * i,
      .range = sizeof(CameraBuffer),
    };
    VkDescriptorBufferInfo& shadow_info = buffer_infos[i * 3 + 1];
    shadow_info = {
      .buffer = shadow_camera_buffer->get(),
      .offset = sizeof(ShadowBuffer) * i,
      .range = sizeof(ShadowBuffer),
    };
    VkDescriptorBufferInfo& frustum_info = buffer_infos[i * 3 + 2];
    frustum_info = {
      .buffer = frustum_buffer->get(),
      .offset = sizeof(FrustumBuffer) * i,
      .range = sizeof(FrustumBuffer),
    };

    std::vector<VkWriteDescriptorSet> writes;
    auto& first = writes.emplace_back();
    first.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    first.dstSet = renderer_descriptor_sets[i];
    first.descriptorCount = 1;
    first.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    first.pBufferInfo = &camera_info;
    first.dstBinding = 0;
    auto& second = writes.emplace_back();
    second.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    second.dstSet = renderer_descriptor_sets[i];
    second.descriptorCount = 1;
    second.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    second.pBufferInfo = &shadow_info;
    second.dstBinding = 1;
    second.pImageInfo = nullptr;
    auto& third = writes.emplace_back();
    third.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    third.dstSet = renderer_descriptor_sets[i];
    third.descriptorCount = 1;
    third.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    third.pBufferInfo = &frustum_info;
    third.dstBinding = 2;
    third.pImageInfo = nullptr;

    vkUpdateDescriptorSets(device->get_device(),
                           static_cast<std::uint32_t>(writes.size()),
                           writes.data(),
                           0,
                           nullptr);
  }
}

Renderer::Renderer(const Device& dev,
                   const BlueprintRegistry& registry,
                   const Window& win)
  : device(&dev)
  , blueprint_registry(&registry)
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

  {
    geometry_image = Image::create(dev,
                                   ImageConfiguration{
                                     .extent = win.framebuffer_size(),
                                     .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                   });

    auto sample_count = device->get_max_sample_count();
    geometry_msaa_image =
      Image::create(dev,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                      .sample_count = sample_count,
                    });
    geometry_depth_image =
      Image::create(dev,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_D32_SFLOAT,
                      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                      .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                      .sample_count = sample_count,
                      .allow_in_ui = false,
                    });
    geometry_material =
      Material::create(*device,
                       blueprint_registry->get("main_geometry"),
                       renderer_descriptor_set_layout);
  }

  {
    z_prepass_material = Material::create(*device,
                                          blueprint_registry->get("z_prepass"),
                                          renderer_descriptor_set_layout);
  }

  {
    line_material = Material::create(
      *device, blueprint_registry->get("line"), renderer_descriptor_set_layout);
  }

  {
    instance_vertex_buffer = std::make_unique<GPUBuffer>(
      dev,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      true);
    static constexpr auto instance_size = sizeof(glm::mat4);
    static constexpr auto instance_count = 1'000'000;
    static constexpr auto instance_size_bytes = instance_size * instance_count;
    const auto bytes = std::make_unique<std::byte[]>(instance_size_bytes);
    instance_vertex_buffer->upload(
      std::span<std::byte>{ bytes.get(), instance_size_bytes });
  }

  {
    gizmo_material = Material::create(*device,
                                      blueprint_registry->get("gizmo"),
                                      renderer_descriptor_set_layout);

    {
      struct GizmoVertex
      {
        glm::vec3 pos;
        glm::vec3 col;
      };
      constexpr std::array<GizmoVertex, 6> gizmo_vertices = {
        GizmoVertex{
          .pos = glm::vec3(0.f, 0.f, 0.f),
          .col = glm::vec3{ 1.0, 0.0, 0.0 },
        },
        {
          glm::vec3(1.f, 0.f, 0.f),
          glm::vec3{ 1.0, 0.0, 0.0 },
        }, // X+ = RED
        {
          glm::vec3(0.f, 0.f, 0.f),
          glm::vec3{ 0.0, 1.0, 0.0 },
        },
        {
          glm::vec3(0.f, 1.f, 0.f),
          glm::vec3{ 0.0, 1.0, 0.0 },
        }, // Y+ = BLUE
        {
          glm::vec3(0.f, 0.f, 0.f),
          glm::vec3{ 0.0, 0.0, 1.0 },
        },
        {
          glm::vec3(0.f, 0.f, 1.f),
          glm::vec3{ 0.0, 0.0, 1.0 },
        }, // Z+ = GREEN
      };

      gizmo_vertex_buffer = std::make_unique<GPUBuffer>(
        dev, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true);
      gizmo_vertex_buffer->upload(std::span{ gizmo_vertices });
    }
  }

  {
    shadow_depth_image =
      Image::create(dev,
                    ImageConfiguration{
                      .extent = { 2048, 2048 },
                      .format = VK_FORMAT_D32_SFLOAT,
                      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                      .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                    });

    shadow_material = Material::create(*device,
                                       blueprint_registry->get("shadow"),
                                       renderer_descriptor_set_layout);
  }

  {
    cull_instances_compute_material =
      Material::create(*device,
                       blueprint_registry->get("compute_culling"),
                       renderer_descriptor_set_layout);

    culled_instance_count_buffer = std::make_unique<GPUBuffer>(
      *device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    static constexpr std::uint32_t zero = 0;
    culled_instance_count_buffer->upload(std::span{ &zero, 1 });

    culled_instance_vertex_buffer = std::make_unique<GPUBuffer>(
      *device,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      true);

    {
      static constexpr auto instance_size = sizeof(glm::mat4);
      static constexpr auto instance_count = 1'000'000;
      static constexpr auto instance_size_bytes =
        instance_size * instance_count;
      const auto bytes = std::make_unique<std::byte[]>(instance_size_bytes);
      culled_instance_vertex_buffer->upload(
        std::span<std::byte>{ bytes.get(), instance_size_bytes });
    }

    cull_instances_compute_material->upload("InstanceInput",
                                            instance_vertex_buffer.get());
    cull_instances_compute_material->upload(
      "InstanceOutput", culled_instance_vertex_buffer.get());
    cull_instances_compute_material->upload("CounterBuffer",
                                            culled_instance_count_buffer.get());
  }
}

auto
Renderer::get_renderer_descriptor_set_layout(Badge<DynamicRendering::App>) const
  -> VkDescriptorSetLayout
{
  return renderer_descriptor_set_layout;
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

  geometry_material.reset();
  z_prepass_material.reset();
  shadow_material.reset();
  line_material.reset();
  gizmo_material.reset();
  cull_instances_compute_material.reset();

  culled_instance_vertex_buffer.reset();
  culled_instance_count_buffer.reset();
  frustum_buffer.reset();
  shadow_camera_buffer.reset();
  shadow_depth_image.reset();
  gizmo_vertex_buffer.reset();
  camera_uniform_buffer.reset();
  geometry_depth_image.reset();
  geometry_image.reset();
  geometry_msaa_image.reset();
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
  draw_commands[cmd].emplace_back(transform);
}

auto
Renderer::submit_lines(const LineDrawCommand& cmd, const glm::mat4& transform)
  -> void
{
  line_draw_commands.emplace_back(cmd, transform);
}

static auto
upload_instance_vertex_data(GPUBuffer& buffer,
                            const auto& draw_commands,
                            const auto& frustum,
                            auto& instance_count_this_frame)
  -> std::vector<std::tuple<DrawCommand, std::uint32_t, std::uint32_t>>
{
  struct InstanceSubmit
  {
    const DrawCommand* cmd;
    InstanceData data;
  };

  std::size_t total_estimated_instances = 0;
  for (const auto& [cmd, instances] : draw_commands)
    total_estimated_instances += instances.size();

  // Preallocate the maximum possible number of instances
  std::vector<InstanceSubmit> filtered_instances(total_estimated_instances);
  std::atomic<std::size_t> filtered_count{ 0 };

  {
    std::vector<std::pair<const DrawCommand*, const std::vector<InstanceData>*>>
      jobs;
    jobs.reserve(draw_commands.size());
    for (const auto& [cmd, instances] : draw_commands)
      jobs.emplace_back(&cmd, &instances);

    // Parallel frustum culling and writing into filtered_instances
    std::for_each(
      std::execution::par_unseq,
      jobs.begin(),
      jobs.end(),
      [&](const auto& job) {
        auto [cmd, instances] = job;
        for (const auto& instance : *instances) {
          const glm::vec3 center_ws = glm::vec3(instance.transform[3]);
          const float radius_ws =
            1.0f * glm::length(glm::vec3(instance.transform[0]));

          if (frustum.intersects(center_ws, radius_ws)) {
            const std::size_t index =
              filtered_count.fetch_add(1, std::memory_order_relaxed);
            filtered_instances[index] = InstanceSubmit{ cmd, instance };
          }
        }
      });
  }

  filtered_instances.resize(filtered_count);
  instance_count_this_frame =
    static_cast<std::uint32_t>(filtered_instances.size());

  if (filtered_instances.empty())
    return {};

  // Flatten instance data to GPU uploadable buffer
  std::vector<InstanceData> instance_data(filtered_instances.size());
  std::transform(std::execution::par_unseq,
                 filtered_instances.begin(),
                 filtered_instances.end(),
                 instance_data.begin(),
                 [](const InstanceSubmit& s) { return s.data; });

  buffer.upload(std::span(instance_data));

  // Map DrawCommand to ranges inside the instance buffer
  std::unordered_map<DrawCommand,
                     std::tuple<std::uint32_t, std::uint32_t>,
                     DrawCommandHasher>
    draw_map;
  for (std::size_t i = 0; i < filtered_instances.size(); ++i) {
    const auto* cmd = filtered_instances[i].cmd;
    auto& [start, count] = draw_map[*cmd];
    if (count == 0)
      start = static_cast<std::uint32_t>(i);
    ++count;
  }

  // Flatten draw map into vector for submission
  std::vector<std::tuple<DrawCommand, std::uint32_t, std::uint32_t>>
    flat_draw_list;
  flat_draw_list.reserve(draw_map.size());
  for (const auto& [cmd, range] : draw_map)
    flat_draw_list.emplace_back(cmd, std::get<0>(range), std::get<1>(range));

  return flat_draw_list;
}

auto
Renderer::update_shadow_buffers(std::uint32_t frame_index) -> void
{
  static constexpr auto calculate_light_view_projection =
    [](const glm::vec3& light_pos) {
      const glm::vec3 center{ 0.f, 0.f, 0.f };
      const glm::vec3 up{ 0.f, 1.f, 0.f };

      const auto view = glm::lookAt(light_pos, center, up);

      constexpr float ortho_size = 50.f;
      constexpr float near_plane = 0.1f;
      constexpr float far_plane = 100.f;

      const auto proj = glm::ortho(-ortho_size,
                                   ortho_size,
                                   -ortho_size,
                                   ortho_size,
                                   near_plane,
                                   far_plane);

      return proj * view;
    };

  glm::mat4 vp =
    calculate_light_view_projection(light_environment.light_position);
  ShadowBuffer shadow_data{
    .light_vp = vp,
    .light_position = glm::vec4{ light_environment.light_position, 1.0F },
    .light_color = glm::vec4{ light_environment.light_color, 1.0F },
  };

  shadow_camera_buffer->upload_with_offset(std::span{ &shadow_data, 1 },
                                           sizeof(ShadowBuffer) * frame_index);
}

auto
Renderer::update_uniform_buffers(std::uint32_t frame_index,
                                 const glm::mat4& vp,
                                 const glm::mat4& inverse_vp) -> void
{
  const CameraBuffer buffer{ .vp = vp, .inverse_vp = inverse_vp };
  camera_uniform_buffer->upload_with_offset(std::span{ &buffer, 1 },
                                            sizeof(CameraBuffer) * frame_index);

  const FrustumBuffer frustum{ current_frustum.planes };
  frustum_buffer->upload_with_offset(std::span{ &frustum, 1 },
                                     sizeof(FrustumBuffer) * frame_index);
}

auto
Renderer::begin_frame(std::uint32_t frame_index,
                      const glm::mat4& vp,
                      const glm::mat4& inverse_vp) -> void
{
  update_uniform_buffers(frame_index, vp, inverse_vp);
  update_shadow_buffers(frame_index);
  update_frustum(vp);
  draw_commands.clear();
}

auto
Renderer::end_frame(std::uint32_t frame_index) -> void
{
  if (draw_commands.empty())
    return;

  {
    auto flat_draw_commands =
      upload_instance_vertex_data(*instance_vertex_buffer,
                                  draw_commands,
                                  current_frustum,
                                  instance_count_this_frame);

    run_culling_compute_pass(frame_index);

    DrawList shadow_casting_draws;
    for (const auto& draw : flat_draw_commands) {
      auto& [cmd, offset, count] = draw;
      if (cmd.casts_shadows)
        shadow_casting_draws.push_back(draw);
    }

    command_buffer->begin_frame(frame_index);

    run_shadow_pass(frame_index, shadow_casting_draws);
    run_z_prepass(frame_index, flat_draw_commands);
    run_geometry_pass(frame_index, flat_draw_commands);
    run_line_pass(frame_index);
    run_gizmo_pass(frame_index);

    command_buffer->submit_and_end(frame_index,
                                   compute_finished_semaphore.at(frame_index));
  }
  draw_commands.clear();
}

auto
Renderer::resize(std::uint32_t width, std::uint32_t height) -> void
{
  geometry_image->resize(width, height);
  geometry_msaa_image->resize(width, height);
  geometry_depth_image->resize(width, height);
}

auto
Renderer::get_output_image() const -> const Image&
{
  return *geometry_image;
}

#pragma region RenderPasses

auto
Renderer::run_z_prepass(std::uint32_t frame_index, const DrawList& draw_list)
  -> void
{
  command_buffer->begin_timer(frame_index, "z_prepass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_to_depth_attachment(
    cmd, geometry_depth_image->get_image());

  VkClearValue depth_clear = { .depthStencil = { 0.0f, 0 } };

  VkRenderingAttachmentInfo depth_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_depth_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = VK_NULL_HANDLE,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = depth_clear,
  };

  VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { .offset = { 0, 0 },
                    .extent = { geometry_image->width(), geometry_image->height() }, },
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 0,
    .pColorAttachments = nullptr,
    .pDepthAttachment = &depth_attachment,
    .pStencilAttachment = nullptr,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(geometry_image->height()),
    .width = static_cast<float>(geometry_image->width()),
    .height = -static_cast<float>(geometry_image->height()),
    .minDepth = 1.f,
    .maxDepth = 0.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);
  auto& z_prepass_pipeline = z_prepass_material->get_pipeline();
  vkCmdBindPipeline(
    cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, z_prepass_pipeline.pipeline);
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          z_prepass_pipeline.layout,
                          0,
                          1,
                          &renderer_descriptor_sets[frame_index],
                          0,
                          nullptr);
  for (const auto& [cmd_info, offset, count] : draw_list) {
    const std::array vertex_buffers = {
      cmd_info.vertex_buffer->get(),
      instance_vertex_buffer->get(),
    };
    constexpr std::array<VkDeviceSize, 2> offsets = { 0ULL, 0ULL };
    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(cmd,
                         cmd_info.index_buffer->get(),
                         0,
                         cmd_info.index_buffer->get_index_type());

    vkCmdDrawIndexed(
      cmd,
      static_cast<std::uint32_t>(cmd_info.index_buffer->get_count()),
      static_cast<std::uint32_t>(count),
      0,
      0,
      offset);
  }

  vkCmdEndRendering(cmd);
  command_buffer->end_timer(frame_index, "z_prepass");
}

auto
Renderer::run_geometry_pass(std::uint32_t frame_index,
                            const DrawList& draw_list) -> void
{
  command_buffer->begin_timer(frame_index, "geometry_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_to_color_attachment(cmd,
                                                geometry_image->get_image());
  CoreUtils::cmd_transition_to_color_attachment(
    cmd, geometry_msaa_image->get_image());

  VkClearValue clear_value = { .color = { { 0.f, 0.f, 0.f, 0.f } } };
  VkRenderingAttachmentInfo color_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_msaa_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT_KHR,
    .resolveImageView = geometry_image->get_view(),
    .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = clear_value
  };

  VkRenderingAttachmentInfo depth_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_depth_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = VK_NULL_HANDLE,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .clearValue = {}, // unused
  };

  const std::array colour_attachments = {
    color_attachment,
  };
  VkRenderingInfo render_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { .offset = { 0, 0 },
                    .extent = { geometry_image->width(), geometry_image->height() }, },
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = static_cast<std::uint32_t>(colour_attachments.size()),
    .pColorAttachments = colour_attachments.data(),
    .pDepthAttachment = &depth_attachment,
    .pStencilAttachment = nullptr,
  };

  vkCmdBeginRendering(cmd, &render_info);

  VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(geometry_image->height()),
    .width = static_cast<float>(geometry_image->width()),
    .height = -static_cast<float>(geometry_image->height()),
    .minDepth = 1.f,
    .maxDepth = 0.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &render_info.renderArea);

  for (const auto& [cmd_info, offset, instance_count] : draw_list) {
    auto&& [vertex_buffer, index_buffer, override_material, shadows] = cmd_info;

    const auto& material =
      override_material ? *override_material : *geometry_material;
    auto& pipeline = material.get_pipeline();

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.layout,
                            0,
                            1,
                            &renderer_descriptor_sets[frame_index],
                            0,
                            nullptr);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    const std::array vertex_buffers = {
      vertex_buffer->get(),
      instance_vertex_buffer->get(),
    };
    constexpr std::array<VkDeviceSize, 2> offsets = { 0ULL, 0ULL };
    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(
      cmd, index_buffer->get(), 0, index_buffer->get_index_type());

    vkCmdDrawIndexed(cmd,
                     static_cast<std::uint32_t>(index_buffer->get_count()),
                     instance_count,
                     0,
                     0,
                     offset);
  }

  vkCmdEndRendering(cmd);
  CoreUtils::cmd_transition_to_shader_read(cmd, geometry_image->get_image());

  command_buffer->end_timer(frame_index, "geometry_pass");
}

auto
Renderer::run_culling_compute_pass(std::uint32_t frame_index) -> void
{
  compute_command_buffer->begin_frame(frame_index);
  compute_command_buffer->begin_timer(frame_index, "cull_instances");

  const auto cmd = compute_command_buffer->get(frame_index);

  // Reset the output counter to zero
  static constexpr std::uint32_t zero = 0;
  culled_instance_count_buffer->upload(std::span{ &zero, 1 });

  auto& cull_pipeline = cull_instances_compute_material->get_pipeline();
  const auto cull_descriptor_set =
    cull_instances_compute_material->prepare_for_rendering(frame_index);

  const std::array descriptor_sets{
    renderer_descriptor_sets[frame_index],
    cull_descriptor_set,
  };

  vkCmdBindPipeline(cmd, cull_pipeline.bind_point, cull_pipeline.pipeline);

  vkCmdBindDescriptorSets(cmd,
                          cull_pipeline.bind_point,
                          cull_pipeline.layout,
                          0,
                          static_cast<std::uint32_t>(descriptor_sets.size()),
                          descriptor_sets.data(),
                          0,
                          nullptr);

  const uint32_t num_instances =
    instance_count_this_frame; // whatever your max is
  const uint32_t group_count = (num_instances + 63) / 64;
  vkCmdDispatch(cmd, group_count, 1, 1);

  compute_command_buffer->end_timer(frame_index, "cull_instances");

  compute_command_buffer->submit_and_end(
    frame_index,
    VK_NULL_HANDLE,
    compute_finished_semaphore.at(frame_index),
    VK_NULL_HANDLE);
}

auto
Renderer::run_line_pass(std::uint32_t frame_index) -> void
{
  command_buffer->begin_timer(frame_index, "line_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = geometry_image->get_image(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
      .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

  VkRenderingAttachmentInfo color_attachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_msaa_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT_KHR,
    .resolveImageView = geometry_image->get_view(),
    .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {}
  };

  VkRenderingAttachmentInfo depth_attachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_depth_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = VK_NULL_HANDLE,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {}
  };

  VkRenderingInfo rendering_info{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { { 0, 0 },
                    { geometry_image->width(), geometry_image->height() } },
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
    .pDepthAttachment = &depth_attachment,
    .pStencilAttachment = nullptr,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  VkViewport viewport{
    .x = 0.f,
    .y = static_cast<float>(geometry_image->height()),
    .width = static_cast<float>(geometry_image->width()),
    .height = -static_cast<float>(geometry_image->height()),
    .minDepth = 1.f,
    .maxDepth = 0.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto& line_pipeline = line_material->get_pipeline();
  vkCmdBindPipeline(
    cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, line_pipeline.pipeline);
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          line_pipeline.layout,
                          0,
                          1,
                          &renderer_descriptor_sets[frame_index],
                          0,
                          nullptr);

  for (const auto& [submission, transform] : line_draw_commands) {
    const std::array<VkDeviceSize, 1> offsets = { 0 };
    const std::array<VkBuffer, 1> buffer = { submission.vertex_buffer->get() };
    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(buffer.size()),
                           buffer.data(),
                           offsets.data());

    vkCmdDraw(cmd, submission.vertex_count, 1, 0, 0);
  }

  vkCmdEndRendering(cmd);

  CoreUtils::cmd_transition_to_shader_read(cmd, geometry_image->get_image());

  command_buffer->end_timer(frame_index, "line_pass");
  line_draw_commands.clear();
}

auto
Renderer::run_gizmo_pass(std::uint32_t frame_index) -> void
{
  command_buffer->begin_timer(frame_index, "gizmo_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);

  VkRenderingAttachmentInfo color_attachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_msaa_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT_KHR,
    .resolveImageView = geometry_image->get_view(),
    .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {}
  };

  VkRenderingAttachmentInfo depth_attachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_depth_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = VK_NULL_HANDLE,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {}
  };

  VkRenderingInfo rendering_info{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { { 0, 0 },
                    { geometry_image->width(), geometry_image->height() } },
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
    .pDepthAttachment = &depth_attachment,
    .pStencilAttachment = nullptr,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  VkViewport viewport{
    .x = 0.f,
    .y = static_cast<float>(geometry_image->height()),
    .width = static_cast<float>(geometry_image->width()),
    .height = -static_cast<float>(geometry_image->height()),
    .minDepth = 1.f,
    .maxDepth = 0.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto& gizmo_pipeline = gizmo_material->get_pipeline();
  vkCmdBindPipeline(
    cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gizmo_pipeline.pipeline);
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          gizmo_pipeline.layout,
                          0,
                          1,
                          &renderer_descriptor_sets[frame_index],
                          0,
                          nullptr);

  const std::array<VkDeviceSize, 1> offsets{ 0 };
  const std::array<VkBuffer, 1> buffers{ gizmo_vertex_buffer->get() };
  vkCmdBindVertexBuffers(cmd, 0, 1, buffers.data(), offsets.data());
  if (glm::mat4 vp{}; camera_uniform_buffer->read_into_with_offset(
        vp, frame_index * sizeof(glm::mat4))) {
    const auto rotation_only = glm::mat4(glm::mat3(vp));

    vkCmdPushConstants(cmd,
                       gizmo_pipeline.layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(glm::mat4),
                       &rotation_only);
  }
  vkCmdDraw(cmd, 6, 1, 0, 0);

  vkCmdEndRendering(cmd);

  command_buffer->end_timer(frame_index, "gizmo_pass");
}

auto
Renderer::run_shadow_pass(std::uint32_t frame_index, const DrawList& draw_list)
  -> void
{
  command_buffer->begin_timer(frame_index, "shadow_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_to_depth_attachment(
    cmd, shadow_depth_image->get_image());

  VkClearValue depth_clear = { .depthStencil = { 0.f, 0 } };

  VkRenderingAttachmentInfo depth_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = shadow_depth_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = VK_NULL_HANDLE,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = depth_clear,
  };

  VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { { 0, 0 },
                    { shadow_depth_image->width(),
                      shadow_depth_image->height() } },
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 0,
    .pColorAttachments = nullptr,
    .pDepthAttachment = &depth_attachment,
    .pStencilAttachment = nullptr,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(shadow_depth_image->height()),
    .width = static_cast<float>(shadow_depth_image->width()),
    .height = -static_cast<float>(shadow_depth_image->height()),
    .minDepth = 0.f,
    .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto& shadow_pipeline = shadow_material->get_pipeline();
  vkCmdBindPipeline(
    cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pipeline.pipeline);
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          shadow_pipeline.layout,
                          0,
                          1,
                          &renderer_descriptor_sets[frame_index],
                          0,
                          nullptr);

  for (const auto& [cmd_info, offset, count] : draw_list) {
    const std::array vertex_buffers = {
      cmd_info.vertex_buffer->get(),
      instance_vertex_buffer->get(),
    };
    constexpr std::array<VkDeviceSize, 2> offsets = { 0ULL, 0ULL };
    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(cmd,
                         cmd_info.index_buffer->get(),
                         0,
                         cmd_info.index_buffer->get_index_type());

    vkCmdDrawIndexed(
      cmd,
      static_cast<std::uint32_t>(cmd_info.index_buffer->get_count()),
      static_cast<std::uint32_t>(count),
      0,
      0,
      offset);
  }

  vkCmdEndRendering(cmd);
  CoreUtils::cmd_transition_depth_to_shader_read(
    cmd, shadow_depth_image->get_image());

  command_buffer->end_timer(frame_index, "shadow_pass");
}

#pragma endregion RenderPasses
