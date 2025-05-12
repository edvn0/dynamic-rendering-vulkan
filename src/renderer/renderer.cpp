#include "renderer/renderer.hpp"

#include "core/image_transition.hpp"
#include "renderer/draw_list_manager.hpp"
#include "renderer/mesh_cache.hpp"

#include <execution>
#include <functional>
#include <future>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <latch>
#include <memory>
#include <vulkan/vulkan.h>

#include <tracy/Tracy.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include "renderer/descriptor_manager.hpp"

#include "core/vulkan_util.hpp"
#include "renderer/mesh.hpp"

#include <glm/gtx/quaternion.hpp>

struct CameraBuffer
{
  alignas(16) const glm::mat4 vp;
  alignas(16) const glm::mat4 inverse_vp;
  alignas(16) const glm::mat4 projection;
  alignas(16) const glm::mat4 view;
  alignas(16) const glm::vec4 camera_position;
  std::array<glm::vec4, 3> padding{};
};
ASSERT_VULKAN_UBO_COMPATIBLE(CameraBuffer);

struct FrustumBuffer
{
  alignas(16) std::array<glm::vec4, 6> planes{};
  alignas(16) std::array<glm::vec4, 2> _padding_{};
};
ASSERT_VULKAN_UBO_COMPATIBLE(FrustumBuffer);

struct ShadowBuffer
{
  alignas(16) glm::mat4 light_vp;
  alignas(16) glm::vec4 light_position;
  alignas(16) glm::vec4 light_color;
  alignas(16) glm::vec4 ambient_color{ 0.1F, 0.1F, 0.1F, 1.0F };
  alignas(16) std::array<glm::vec4, 1> padding{};
};
ASSERT_VULKAN_UBO_COMPATIBLE(ShadowBuffer);

template<typename T, std::size_t N = frames_in_flight>
static constexpr auto
create_sized_array(const T& value) -> std::array<T, N>
{
  std::array<T, N> arr{};
  arr.fill(value);
  return arr;
}

static constexpr auto stages = VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT |
                               VK_SHADER_STAGE_COMPUTE_BIT;

static constexpr std::array renderer_bindings_metadata{
  DescriptorBindingMetadata{
    .binding = 0,
    .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .stage_flags = stages,
    .name = "camera_ubo",
  },
  DescriptorBindingMetadata{
    .binding = 1,
    .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .stage_flags = stages,
    .name = "shadow_camera_ubo",
  },
  DescriptorBindingMetadata{
    .binding = 2,
    .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .stage_flags = stages,
    .name = "frustum_ubo",
  },
  DescriptorBindingMetadata{
    .binding = 3,
    .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .name = "shadow_depth",
  },
};

auto
to_renderpass(const std::string_view name) -> RenderPass
{
  if (name == "main_geometry") {
    return RenderPass::MainGeometry;
  }
  if (name == "shadow") {
    return RenderPass::Shadow;
  }
  if (name == "line") {
    return RenderPass::Line;
  }
  if (name == "z_prepass") {
    return RenderPass::ZPrepass;
  }
  if (name == "colour_correction") {
    return RenderPass::ColourCorrection;
  }
  if (name == "compute_culling") {
    return RenderPass::ComputeCulling;
  }
  if (name == "skybox") {
    return RenderPass::Skybox;
  }

  assert(false && "Unknown render pass name");
  return RenderPass::MainGeometry;
}

Renderer::Renderer(const Device& dev,
                   const BlueprintRegistry& registry,
                   const Window& win,
                   BS::priority_thread_pool& p)
  : device(&dev)
  , blueprint_registry(&registry)
  , thread_pool(&p)
{
  DescriptorLayoutBuilder builder(renderer_bindings_metadata);
  descriptor_set_manager =
    std::make_unique<DescriptorSetManager>(*device, std::move(builder));

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

    auto result =
      Material::create(*device, blueprint_registry->get("main_geometry"));
    if (result.has_value()) {
      geometry_material = std::move(result.value());
    } else {
      assert(false && "Failed to create main geometry material.");
    }
  }

  {
    // Environment map
    skybox_image = Image::load_cubemap(*device, "sf.ktx2");
    if (!skybox_image) {
      assert(false && "Failed to load environment map.");
    }

    auto result = Material::create(*device, blueprint_registry->get("skybox"));
    if (!result.has_value()) {
      assert(false && "Failed to create environment map material.");
    }

    skybox_material = std::move(result.value());
    skybox_material->upload("skybox_sampler", skybox_image.get());

    skybox_attachment_texture =
      Image::create(*device,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                      .debug_name = "skybox_attachment_texture",
                    });
  }

  {
    composite_attachment_texture = Image::create(
      *device,
      ImageConfiguration{ .extent = win.framebuffer_size(),
                          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                          .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_SAMPLED_BIT,
                          .debug_name = "composite_attachment_texture" });

    auto result =
      Material::create(*device, blueprint_registry->get("composite"));
    if (result.has_value()) {
      composite_attachment_material = std::move(result.value());
    } else {
      assert(false && "Failed to create composite material.");
    }

    composite_attachment_material->upload("skybox_input",
                                          skybox_attachment_texture.get());
    composite_attachment_material->upload("geometry_input",
                                          geometry_image.get());
  }

  {
    colour_corrected_image =
      Image::create(*device,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_B8G8R8A8_SRGB,
                      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                    });

    auto result =
      Material::create(*device, blueprint_registry->get("colour_correction"));
    if (result.has_value()) {
      colour_corrected_material = std::move(result.value());
    } else {
      assert(false && "Failed to create main geometry material.");
    }

    colour_corrected_material->upload("input_image",
                                      composite_attachment_texture.get());
  }

  {
    auto result =
      Material::create(*device, blueprint_registry->get("z_prepass"));
    if (result.has_value()) {
      z_prepass_material = std::move(result.value());
    } else {
      assert(false && "Failed to create z prepass material.");
    }
  }

  {
    auto result = Material::create(*device, blueprint_registry->get("line"));
    if (result.has_value()) {
      line_material = std::move(result.value());
    } else {
      assert(false && "Failed to create line material.");
    }

    line_instance_buffer =
      std::make_unique<GPUBuffer>(*device,
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  true,
                                  "line_instance_vertex_buffer");

    auto&& [bytes, instance_size_bytes] =
      make_bytes<LineInstanceData, 100'000>();
    if (!bytes) {
      assert(false && "Failed to allocate line instance buffer.");
    }
    line_instance_buffer->upload(
      std::span<std::byte>{ bytes.get(), instance_size_bytes });
  }

  {
    instance_vertex_buffer = std::make_unique<GPUBuffer>(
      dev,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      true,
      "geometry_instance_vertex_buffer");
    auto&& [bytes, instance_size_bytes] = make_bytes<InstanceData, 1'000'000>();
    if (!bytes) {
      assert(false && "Failed to allocate instance vertex buffer.");
    }
    instance_vertex_buffer->upload(
      std::span{ bytes.get(), instance_size_bytes });
  }

  {
    instance_shadow_vertex_buffer = std::make_unique<GPUBuffer>(
      dev,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      true,
      "geometry_instance_shadow_vertex_buffer");
    auto&& [bytes, instance_size_bytes] = make_bytes<InstanceData, 1'000'000>();
    if (!bytes) {
      assert(false && "Failed to allocate instance vertex buffer.");
    }
    instance_shadow_vertex_buffer->upload(
      std::span{ bytes.get(), instance_size_bytes });
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
                      .debug_name = "shadow_depth",
                    });

    auto result = Material::create(*device, blueprint_registry->get("shadow"));
    if (result.has_value()) {
      shadow_material = std::move(result.value());
    } else {
      assert(false && "Failed to create shadow material.");
    }
  }

  {
    auto result =
      Material::create(*device, blueprint_registry->get("compute_culling"));

    if (result.has_value()) {
      cull_instances_compute_material = std::move(result.value());
    } else {
      assert(false && "Failed to create compute culling material.");
    }

    culled_instance_count_buffer = std::make_unique<GPUBuffer>(
      *device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    static constexpr std::uint32_t zero = 0;
    culled_instance_count_buffer->upload(std::span{ &zero, 1 });

    culled_instance_vertex_buffer = std::make_unique<GPUBuffer>(
      *device,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      true);

    {
      // This was a glm::mat4 before, lets strongly type instead.
      auto&& [bytes, instance_size_bytes] =
        make_bytes<InstanceData, 1'000'000>();
      culled_instance_vertex_buffer->upload(
        std::span{ bytes.get(), instance_size_bytes });
    }

    cull_instances_compute_material->upload("InstanceInput",
                                            instance_vertex_buffer.get());
    cull_instances_compute_material->upload(
      "InstanceOutput", culled_instance_vertex_buffer.get());
    cull_instances_compute_material->upload("CounterBuffer",
                                            culled_instance_count_buffer.get());
  }

  camera_uniform_buffer =
    GPUBuffer::zero_initialise(*device,
                               sizeof(CameraBuffer) * frames_in_flight,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               true,
                               "camera_ubo");
  shadow_camera_buffer =
    GPUBuffer::zero_initialise(*device,
                               sizeof(ShadowBuffer) * frames_in_flight,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               true,
                               "shadow_camera_ubo");
  frustum_buffer =
    GPUBuffer::zero_initialise(*device,
                               sizeof(FrustumBuffer) * frames_in_flight,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               true,
                               "frustum_ubo");

  std::array uniforms{ camera_uniform_buffer.get(),
                       shadow_camera_buffer.get(),
                       frustum_buffer.get() };
  std::array images{ shadow_depth_image.get() };
  descriptor_set_manager->allocate_sets(std::span(uniforms), std::span(images));
}

auto
Renderer::get_renderer_descriptor_set_layout(Badge<AssetReloader>) const
  -> VkDescriptorSetLayout
{
  return descriptor_set_manager->get_layout();
}

auto
Renderer::destroy() -> void
{
  for (const auto& semaphore : compute_finished_semaphore) {
    vkDestroySemaphore(device->get_device(), semaphore, nullptr);
  }
}

Renderer::~Renderer()
{
  destroy();
}

auto
Renderer::submit(const DrawCommand& cmd, const glm::mat4& transform) -> void
{
  for (auto& submesh : cmd.mesh->get_submeshes()) {
    auto command = DrawCommand{
      .mesh = cmd.mesh,
      .override_material = cmd.override_material,
      .submesh_index = cmd.mesh->get_submesh_index(submesh),
      .casts_shadows = cmd.casts_shadows,
    };
    draw_commands[command].emplace_back(transform);
    if (cmd.casts_shadows) {
      shadow_draw_commands[command].emplace_back(transform);
    }
  }
}

auto
Renderer::submit_lines(const glm::vec3& start,
                       const glm::vec3& end,
                       const float width,
                       const glm::vec4& colour) -> void
{
  std::uint32_t packed_color =
    (static_cast<std::uint32_t>(colour.a * 255.0f) << 24) |
    (static_cast<std::uint32_t>(colour.b * 255.0f) << 16) |
    (static_cast<std::uint32_t>(colour.g * 255.0f) << 8) |
    (static_cast<std::uint32_t>(colour.r * 255.0f) << 0);

  line_instances.emplace_back(start, width, end, packed_color);
}

static auto
upload_instance_vertex_data(auto& pool,
                            GPUBuffer& buffer,
                            const auto& draw_commands,
                            const auto& frustum,
                            std::integral auto& instance_count_this_frame)
  -> DrawList
{
  std::size_t total_estimated_instances = 0;
  for (const auto& [cmd, instances] : draw_commands)
    total_estimated_instances += instances.size();

  // Preallocate the maximum possible number of instances
  std::vector<DrawInstanceSubmit> filtered_instances(total_estimated_instances);
  std::atomic<std::size_t> filtered_count{ 0 };

  {
    std::vector<std::pair<const DrawCommand*, const std::vector<InstanceData>*>>
      jobs;
    jobs.reserve(draw_commands.size());
    for (const auto& [cmd, instances] : draw_commands)
      jobs.emplace_back(&cmd, &instances);

    // Parallel frustum culling
    auto fut = pool.submit_loop(0, jobs.size(), [&](std::size_t i) {
      const auto& [cmd, instances] = jobs[i];
      for (const auto& instance : *instances) {
        const auto center_ws = glm::vec3(instance.transform[3]);
        const float radius_ws =
          1.0f * glm::length(glm::vec3(instance.transform[0]));

        if (frustum.intersects(center_ws, radius_ws)) {
          const std::size_t index =
            filtered_count.fetch_add(1, std::memory_order_relaxed);
          filtered_instances[index] = { cmd, instance };
        }
      }
    });

    fut.wait();
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
                 [](const DrawInstanceSubmit& s) { return s.data; });

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
  DrawList flat_draw_list;
  flat_draw_list.reserve(draw_map.size());
  for (const auto& [cmd, range] : draw_map)
    flat_draw_list.emplace_back(cmd, std::get<0>(range), std::get<1>(range));

  return flat_draw_list;
}

auto
Renderer::upload_line_instance_data() -> void
{
  line_instance_count_this_frame =
    static_cast<std::uint32_t>(line_instances.size());
  if (line_instance_count_this_frame == 0)
    return;

  line_instance_buffer->upload(std::span(line_instances));
  line_instances.clear();
}

auto
Renderer::update_shadow_buffers(std::uint32_t frame_index) -> void
{
  static constexpr auto calculate_light_view_projection =
    [](const glm::vec3& light_pos) {
      const glm::vec3 center{ 0.f, 0.f, 0.f };
      const glm::vec3 up{ 0.f, 1.f, 0.f };

      const auto view = glm::lookAtRH(light_pos, center, up);

      constexpr float ortho_size = 50.f;
      constexpr float near_plane = 0.1f;
      constexpr float far_plane = 100.f;

      const glm::mat4 proj = glm::orthoRH_ZO(-ortho_size,
                                             ortho_size,
                                             -ortho_size,
                                             ortho_size,
                                             far_plane,
                                             near_plane);

      return proj * view;
    };

  const glm::mat4 vp =
    calculate_light_view_projection(light_environment.light_position);
  ShadowBuffer shadow_data{
    .light_vp = vp,
    .light_position = glm::vec4{ light_environment.light_position, 1.0F },
    .light_color = light_environment.light_color,
    .ambient_color = light_environment.ambient_color,
  };

  shadow_camera_buffer->upload_with_offset(std::span{ &shadow_data, 1 },
                                           sizeof(ShadowBuffer) * frame_index);
  light_frustum.update(shadow_data.light_vp);
}

auto
Renderer::update_uniform_buffers(const std::uint32_t frame_index,
                                 const glm::mat4& view,
                                 const glm::mat4& projection,
                                 const glm::mat4& inverse_projection,
                                 const glm::vec3& camera_position) const -> void
{
  const CameraBuffer buffer{
    .vp = projection * view,
    .inverse_vp = inverse_projection * view,
    .projection = projection,
    .view = view,
    .camera_position = { camera_position, 1.0F },
  };
  camera_uniform_buffer->upload_with_offset(std::span{ &buffer, 1 },
                                            sizeof(CameraBuffer) * frame_index);

  const FrustumBuffer frustum{ camera_frustum.planes };
  frustum_buffer->upload_with_offset(std::span{ &frustum, 1 },
                                     sizeof(FrustumBuffer) * frame_index);
}

auto
Renderer::begin_frame(std::uint32_t frame_index, const VP& matrices) -> void
{
  const auto vp = matrices.projection * matrices.view;
  const auto inverse_vp = matrices.inverse_projection * matrices.view;
  const auto position = matrices.view[3];
  update_uniform_buffers(frame_index,
                         matrices.view,
                         matrices.projection,
                         matrices.inverse_projection,
                         position);
  update_shadow_buffers(frame_index);
  update_frustum(vp);
  draw_commands.clear();
  shadow_draw_commands.clear();
}

auto
Renderer::end_frame(std::uint32_t frame_index) -> void
{
  ZoneScoped;

  if ((shadow_draw_commands.empty() && draw_commands.empty()))
    return;

  DrawList flat_shadow_draw_commands;
  DrawList flat_draw_commands;
  std::size_t shadow_count{ 0 };

  std::latch uploads_remaining(3);

  constexpr std::size_t culling_threshold = 500;
  std::size_t total_instance_count = 0;
  {
    ZoneScopedN("Count Instances");
    total_instance_count = std::transform_reduce(
      draw_commands.begin(),
      draw_commands.end(),
      0ULL,
      std::plus<>{},
      [](const auto& pair) { return pair.second.size(); });
  }

  auto&& [should_geom_cull, should_shadow_cull] = submit_tuple_and_wait(
    *thread_pool,
    [&] {
      return DrawListManager::should_perform_culling(draw_commands,
                                                     culling_threshold);
    },
    [&] {
      return DrawListManager::should_perform_culling(shadow_draw_commands,
                                                     culling_threshold);
    });

  if (should_geom_cull) {
    thread_pool->detach_task([this, &flat_draw_commands, &uploads_remaining] {
      ZoneScopedN("Instance Upload");
      flat_draw_commands =
        upload_instance_vertex_data(*thread_pool,
                                    *instance_vertex_buffer,
                                    draw_commands,
                                    camera_frustum,
                                    instance_count_this_frame);
      uploads_remaining.count_down();
    });
  } else {
    thread_pool->detach_task([this, &flat_draw_commands, &uploads_remaining] {
      ZoneScopedN("Flat Upload (no cull)");
      flat_draw_commands = DrawListManager::flatten_draw_commands(
        draw_commands, *instance_vertex_buffer, instance_count_this_frame);
      uploads_remaining.count_down();
    });
  }

  if (should_shadow_cull) {
    thread_pool->detach_task(
      [this, &flat_shadow_draw_commands, &shadow_count, &uploads_remaining] {
        ZoneScopedN("Shadow Upload");
        flat_shadow_draw_commands =
          upload_instance_vertex_data(*thread_pool,
                                      *instance_shadow_vertex_buffer,
                                      shadow_draw_commands,
                                      light_frustum,
                                      shadow_count);
        uploads_remaining.count_down();
      });
  } else {
    std::uint32_t temp_count = 0;
    thread_pool->detach_task([this,
                              &flat_shadow_draw_commands,
                              &temp_count,
                              &uploads_remaining,
                              &shadow_count] {
      ZoneScopedN("Flat Shadow Upload (no cull)");
      flat_shadow_draw_commands = DrawListManager::flatten_draw_commands(
        shadow_draw_commands, *instance_shadow_vertex_buffer, temp_count);
      uploads_remaining.count_down();
      shadow_count = temp_count;
    });
  }

  thread_pool->detach_task([this, &uploads = uploads_remaining] {
    ZoneScopedN("Line instance upload");
    upload_line_instance_data();
    uploads.count_down();
  });

  uploads_remaining.wait();

  command_buffer->begin_frame(frame_index);
  if (total_instance_count >= culling_threshold) {
    run_culling_compute_pass(frame_index);
  }

  run_skybox_pass(frame_index);
  run_shadow_pass(frame_index, flat_shadow_draw_commands);
  run_z_prepass(frame_index, flat_draw_commands);
  run_geometry_pass(frame_index, flat_draw_commands);
  run_line_pass(frame_index);

  run_composite_pass(frame_index);
  run_postprocess_passes(frame_index);

  const auto semaphore = total_instance_count >= culling_threshold
                           ? compute_finished_semaphore.at(frame_index)
                           : VK_NULL_HANDLE;
  command_buffer->submit_and_end(frame_index, semaphore);

  draw_commands.clear();
}

auto
Renderer::resize(std::uint32_t width, std::uint32_t height) -> void
{
  geometry_image->resize(width, height);
  geometry_msaa_image->resize(width, height);
  geometry_depth_image->resize(width, height);
  skybox_attachment_texture->resize(width, height);
  composite_attachment_texture->resize(width, height);
  colour_corrected_image->resize(width, height);

  skybox_material->invalidate(skybox_attachment_texture.get());
  composite_attachment_material->invalidate(skybox_attachment_texture.get());
  composite_attachment_material->invalidate(geometry_image.get());
  colour_corrected_material->invalidate(composite_attachment_texture.get());
}

auto
Renderer::get_output_image() const -> const Image&
{
  return *colour_corrected_image;
}

#pragma region RenderPasses

auto
Renderer::run_skybox_pass(std::uint32_t frame_index) -> void
{
  ZoneScopedN("Skybox pass");

  command_buffer->begin_timer(frame_index, "skybox_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);

  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = skybox_attachment_texture->get_image(),
      .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .src_access_mask = 0,
      .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

  const VkClearValue clear_value = { .color = { { 0.f, 0.f, 0.f, 0.f } } };
  VkRenderingAttachmentInfo color_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = skybox_attachment_texture->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = clear_value,
  };

  const VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { { 0, 0 },
                    { skybox_attachment_texture->width(),
                      skybox_attachment_texture->height() } },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  const VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(skybox_attachment_texture->height()),
    .width = static_cast<float>(skybox_attachment_texture->width()),
    .height = -static_cast<float>(skybox_attachment_texture->height()),
    .minDepth = 0.f,
    .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto& pipeline = skybox_material->get_pipeline();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

  const std::array descriptor_sets = {
    descriptor_set_manager->get_set(frame_index),
    skybox_material->prepare_for_rendering(frame_index),
  };
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline.layout,
                          0,
                          static_cast<std::uint32_t>(descriptor_sets.size()),
                          descriptor_sets.data(),
                          0,
                          nullptr);

  auto mesh_expected = MeshCache::the().get_mesh<MeshType::CubeOnlyPosition>();
  if (mesh_expected.has_value()) {
    auto& mesh = mesh_expected.value();
    const auto& vertex_buffer = mesh->get_vertex_buffer();
    const auto& index_buffer = mesh->get_index_buffer();

    const std::array vertex_buffers = {
      vertex_buffer->get(),
    };
    constexpr std::array<VkDeviceSize, 1> offsets = { 0 };
    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(
      cmd, index_buffer->get(), 0, index_buffer->get_index_type());

    vkCmdDrawIndexed(
      cmd, static_cast<std::uint32_t>(index_buffer->get_count()), 1, 0, 0, 0);
  }

  vkCmdEndRendering(cmd);

  CoreUtils::cmd_transition_to_shader_read(
    cmd, skybox_attachment_texture->get_image());

  command_buffer->end_timer(frame_index, "skybox_pass");
}

auto
Renderer::run_z_prepass(std::uint32_t frame_index, const DrawList& draw_list)
  -> void
{
  ZoneScopedN("Z Prepass");

  command_buffer->begin_timer(frame_index, "z_prepass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_to_depth_attachment(
    cmd, geometry_depth_image->get_image());

  const VkClearValue depth_clear = { .depthStencil = { 0.0f, 0 } };

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

  const VkRenderingInfo rendering_info = {
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

  const VkViewport viewport = {
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

  std::vector sets = { descriptor_set_manager->get_set(frame_index) };
  if (const auto set = z_prepass_material->prepare_for_rendering(frame_index)) {
    sets.push_back(set);
  }
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          z_prepass_pipeline.layout,
                          0,
                          1,
                          &descriptor_set_manager->get_set(frame_index),
                          0,
                          nullptr);
  for (const auto& [cmd_info, offset, count] : draw_list) {
    const auto& vertex_buffer = cmd_info.mesh->get_vertex_buffer();
    const auto& index_buffer = cmd_info.mesh->get_index_buffer();

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
                     count,
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
  ZoneScopedN("Geometry pass");

  command_buffer->begin_timer(frame_index, "geometry_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = geometry_image->get_image(),
      .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
      .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

  constexpr VkClearValue clear_value = { .color = { { 0.f, 0.f, 0.f, 0.f } } };
  const VkRenderingAttachmentInfo color_attachment = {
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
  const VkRenderingInfo render_info = {
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

  const VkViewport viewport = {
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
    const auto& vertex_buffer = cmd_info.mesh->get_vertex_buffer();
    const auto& index_buffer = cmd_info.mesh->get_index_buffer();
    const auto& submesh_material =
      cmd_info.mesh->get_material_by_submesh_index(cmd_info.submesh_index);

    auto& material = cmd_info.override_material ? *cmd_info.override_material
                                                : *submesh_material;
    auto& pipeline = material.get_pipeline();

    const auto& material_set = material.prepare_for_rendering(frame_index);

    std::array descriptor_sets{
      descriptor_set_manager->get_set(frame_index),
      material_set,
    };
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.layout,
                            0,
                            static_cast<std::uint32_t>(descriptor_sets.size()),
                            descriptor_sets.data(),
                            0,
                            nullptr);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    const std::array vertex_buffers = {
      vertex_buffer->get(),
      instance_vertex_buffer->get(),
    };
    constexpr std::array offsets = { 0ULL, 0ULL };
    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(
      cmd, index_buffer->get(), 0, index_buffer->get_index_type());

    auto&& [pc_stage, pc_offset, pc_size, pc_pointer] =
      material.generate_push_constant_data();
    vkCmdPushConstants(
      cmd, pipeline.layout, pc_stage, pc_offset, pc_size, pc_pointer);
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
  ZoneScopedN("Compute pass");

  compute_command_buffer->begin_frame(frame_index);
  compute_command_buffer->begin_timer(frame_index, "cull_instances");

  const auto cmd = compute_command_buffer->get(frame_index);

  static constexpr std::uint32_t zero = 0;
  culled_instance_count_buffer->upload(std::span{ &zero, 1 });

  auto& cull_pipeline = cull_instances_compute_material->get_pipeline();
  const auto cull_descriptor_set =
    cull_instances_compute_material->prepare_for_rendering(frame_index);

  const std::array descriptor_sets{
    descriptor_set_manager->get_set(frame_index),
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
  ZoneScopedN("Line pass");

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

  const VkRenderingInfo rendering_info{
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

  const VkViewport viewport{
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
                          &descriptor_set_manager->get_set(frame_index),
                          0,
                          nullptr);

  const std::array<VkDeviceSize, 1> offsets = { 0 };
  const std::array<VkBuffer, 1> buffers = { line_instance_buffer->get() };
  vkCmdBindVertexBuffers(cmd, 0, 1, buffers.data(), offsets.data());

  vkCmdDraw(cmd, 4, line_instance_count_this_frame, 0, 0);

  vkCmdEndRendering(cmd);
  CoreUtils::cmd_transition_to_shader_read(cmd, geometry_image->get_image());
  command_buffer->end_timer(frame_index, "line_pass");
}

auto
Renderer::run_shadow_pass(std::uint32_t frame_index, const DrawList& draw_list)
  -> void
{
  ZoneScopedN("Shadow pass");

  command_buffer->begin_timer(frame_index, "shadow_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_to_depth_attachment(
    cmd, shadow_depth_image->get_image());

  constexpr VkClearValue depth_clear = { .depthStencil = { 0.f, 0 } };

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

  const VkRenderingInfo rendering_info = {
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

  const VkViewport viewport = {
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

  std::vector sets{
    descriptor_set_manager->get_set(frame_index),
  };
  if (const auto set = shadow_material->prepare_for_rendering(frame_index)) {
    sets.push_back(set);
  }
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          shadow_pipeline.layout,
                          0,
                          static_cast<std::uint32_t>(sets.size()),
                          sets.data(),
                          0,
                          nullptr);

  for (const auto& [cmd_info, offset, count] : draw_list) {
    const auto& vertex_buffer = cmd_info.mesh->get_vertex_buffer();
    const auto& index_buffer = cmd_info.mesh->get_index_buffer();

    const std::array vertex_buffers = {
      vertex_buffer->get(),
      instance_shadow_vertex_buffer->get(),
    };
    constexpr std::array offsets = { 0ULL, 0ULL };
    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(
      cmd, index_buffer->get(), 0, index_buffer->get_index_type());

    vkCmdDrawIndexed(cmd,
                     static_cast<std::uint32_t>(index_buffer->get_count()),
                     count,
                     0,
                     0,
                     offset);
  }

  vkCmdEndRendering(cmd);
  CoreUtils::cmd_transition_depth_to_shader_read(
    cmd, shadow_depth_image->get_image());

  command_buffer->end_timer(frame_index, "shadow_pass");
}

auto
Renderer::run_colour_correction_pass(std::uint32_t frame_index) -> void
{
  ZoneScopedN("Colour correction pass");

  command_buffer->begin_timer(frame_index, "colour_correction_pass");

  const auto cmd = command_buffer->get(frame_index);
  CoreUtils::cmd_transition_image(
    cmd,
    {
      .image = colour_corrected_image->get_image(),
      .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
      .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .src_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

  const VkClearValue clear_value = { .color = { { 0.F, 0.F, 0.F, 0.F } } };
  VkRenderingAttachmentInfo color_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = colour_corrected_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = clear_value,
  };

  const VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { { 0, 0 },
                    { colour_corrected_image->width(),
                      colour_corrected_image->height() } },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  const VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(colour_corrected_image->height()),
    .width = static_cast<float>(colour_corrected_image->width()),
    .height = -static_cast<float>(colour_corrected_image->height()),
    .minDepth = 0.f,
    .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto* material = colour_corrected_material.get();
  const auto& pipeline = material->get_pipeline();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

  const auto& descriptor_set = material->prepare_for_rendering(frame_index);

  const std::array descriptor_sets{
    descriptor_set_manager->get_set(frame_index),
    descriptor_set,
  };
  vkCmdBindDescriptorSets(cmd,
                          pipeline.bind_point,
                          pipeline.layout,
                          0,
                          static_cast<std::uint32_t>(descriptor_sets.size()),
                          descriptor_sets.data(),
                          0,
                          nullptr);

  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);

  CoreUtils::cmd_transition_to_shader_read(cmd,
                                           colour_corrected_image->get_image());
  command_buffer->end_timer(frame_index, "colour_correction_pass");
}

auto
Renderer::run_composite_pass(std::uint32_t frame_index) -> void
{
  ZoneScopedN("Composite pass");

  command_buffer->begin_timer(frame_index, "composite_pass");

  const VkCommandBuffer cmd = command_buffer->get(frame_index);

  CoreUtils::cmd_transition_to_color_attachment(
    cmd, composite_attachment_texture->get_image());

  const VkClearValue clear_value = { .color = { { 0.F, 0.F, 0.F, 0.F } } };
  VkRenderingAttachmentInfo color_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = composite_attachment_texture->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = clear_value,
  };

  const VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { { 0, 0 },
                    { composite_attachment_texture->width(),
                      composite_attachment_texture->height() } },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  const VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(composite_attachment_texture->height()),
    .width = static_cast<float>(composite_attachment_texture->width()),
    .height = -static_cast<float>(composite_attachment_texture->height()),
    .minDepth = 0.f,
    .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto* material = composite_attachment_material.get();
  const auto& pipeline = material->get_pipeline();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

  const auto descriptor_set = material->prepare_for_rendering(frame_index);
  const std::array descriptor_sets{
    descriptor_set_manager->get_set(frame_index),
    descriptor_set,
  };
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline.layout,
                          0,
                          static_cast<std::uint32_t>(descriptor_sets.size()),
                          descriptor_sets.data(),
                          0,
                          nullptr);

  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);

  CoreUtils::cmd_transition_to_shader_read(
    cmd, composite_attachment_texture->get_image());

  command_buffer->end_timer(frame_index, "composite_pass");
}

#pragma endregion RenderPasses
