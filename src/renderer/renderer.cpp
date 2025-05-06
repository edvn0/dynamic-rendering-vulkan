#include "renderer/renderer.hpp"

#include "core/image_transition.hpp"

#include <execution>
#include <functional>
#include <future>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <vulkan/vulkan.h>

#include <tracy/Tracy.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <latch>

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
  glm::vec4 ambient_color{ 0.1F, 0.1F, 0.1F, 1.0F };
  std::array<glm::vec4, 1> padding{};
};

template<typename T, std::size_t N = image_count>
static constexpr auto
create_sized_array(const T& value) -> std::array<T, N>
{
  std::array<T, N> arr{};
  arr.fill(value);
  return arr;
}

struct DescriptorBindingMetadata
{
  uint32_t binding;
  VkDescriptorType descriptor_type;
  VkShaderStageFlags stage_flags;
  std::string_view name;
  GPUBuffer* buffer{ nullptr };
  std::size_t element_size{ 0 };
};
static std::array<DescriptorBindingMetadata, 4> renderer_bindings_metadata{
  DescriptorBindingMetadata{
    .binding = 0,
    .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                   VK_SHADER_STAGE_COMPUTE_BIT,
    .name = "camera_ubo",
  },
  DescriptorBindingMetadata{
    .binding = 1,
    .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    .name = "shadow_camera_ubo",
  },
  DescriptorBindingMetadata{
    .binding = 2,
    .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                   VK_SHADER_STAGE_COMPUTE_BIT,
    .name = "frustum_ubo",
  },
  DescriptorBindingMetadata{
    .binding = 3,
    .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .name = "shadow_image",
  },
};

auto
Renderer::create_descriptor_set_layout_from_metadata() -> void
{
  camera_uniform_buffer = std::make_unique<GPUBuffer>(
    *device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, "camera_ubo");
  shadow_camera_buffer = std::make_unique<GPUBuffer>(
    *device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, "shadow_camera_ubo");
  frustum_buffer = std::make_unique<GPUBuffer>(
    *device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, "frustum_ubo");

  for (auto& meta : renderer_bindings_metadata) {
    if (meta.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
      auto size = [&]() -> std::size_t {
        if (meta.name == "camera_ubo")
          return sizeof(CameraBuffer);
        if (meta.name == "shadow_camera_ubo")
          return sizeof(ShadowBuffer);
        if (meta.name == "frustum_ubo")
          return sizeof(FrustumBuffer);
        return 0;
      }();

      auto buf = std::make_unique<GPUBuffer>(*device,
                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                             true,
                                             std::string(meta.name).c_str());

      meta.buffer = buf.get();
      meta.element_size = size;

      if (meta.name == "camera_ubo")
        camera_uniform_buffer = std::move(buf);
      if (meta.name == "shadow_camera_ubo")
        shadow_camera_buffer = std::move(buf);
      if (meta.name == "frustum_ubo")
        frustum_buffer = std::move(buf);
    }
  }

  std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
  for (const auto& meta : renderer_bindings_metadata) {
    layout_bindings.push_back({
      .binding = meta.binding,
      .descriptorType = meta.descriptor_type,
      .descriptorCount = 1,
      .stageFlags = meta.stage_flags,
    });
  }

  VkDescriptorSetLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
    .pBindings = layout_bindings.data(),
  };

  vkCreateDescriptorSetLayout(device->get_device(),
                              &layout_info,
                              nullptr,
                              &renderer_descriptor_set_layout);
}

auto
Renderer::finalize_renderer_descriptor_sets() -> void
{
  for (const auto& meta : renderer_bindings_metadata) {
    if (meta.buffer && meta.element_size > 0) {
      const std::size_t total_bytes = meta.element_size * image_count;
      auto zero_init = std::make_unique<std::byte[]>(total_bytes);
      std::memset(zero_init.get(), 0, total_bytes);
      meta.buffer->upload(std::span{ zero_init.get(), total_bytes });
    }
  }

  std::vector<VkDescriptorPoolSize> pool_sizes;
  for (const auto& meta : renderer_bindings_metadata) {
    pool_sizes.push_back({
      .type = meta.descriptor_type,
      .descriptorCount = image_count,
    });
  }

  VkDescriptorPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = image_count,
    .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  };

  vkCreateDescriptorPool(
    device->get_device(), &pool_info, nullptr, &descriptor_pool);

  const auto layouts = create_sized_array(renderer_descriptor_set_layout);
  VkDescriptorSetAllocateInfo alloc_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = descriptor_pool,
    .descriptorSetCount = image_count,
    .pSetLayouts = layouts.data(),
  };

  vkAllocateDescriptorSets(
    device->get_device(), &alloc_info, renderer_descriptor_sets.data());

  const auto total_bindings = renderer_bindings_metadata.size() * image_count;

  std::vector<VkWriteDescriptorSet> write_sets(total_bindings);
  std::vector<VkDescriptorBufferInfo> buffer_infos(total_bindings);
  std::vector<VkDescriptorImageInfo> image_infos(total_bindings);

  for (std::size_t i = 0; i < image_count; ++i) {
    for (std::size_t j = 0; j < renderer_bindings_metadata.size(); ++j) {
      const auto& meta = renderer_bindings_metadata[j];
      const std::size_t index = i * renderer_bindings_metadata.size() + j;

      auto& write = write_sets[index];
      write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = renderer_descriptor_sets[i],
        .dstBinding = meta.binding,
        .descriptorCount = 1,
        .descriptorType = meta.descriptor_type,
      };

      if (meta.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        GPUBuffer* buf = nullptr;
        std::size_t size = 0;
        if (meta.name == "camera_ubo") {
          buf = camera_uniform_buffer.get();
          size = sizeof(CameraBuffer);
        } else if (meta.name == "shadow_camera_ubo") {
          buf = shadow_camera_buffer.get();
          size = sizeof(ShadowBuffer);
        } else if (meta.name == "frustum_ubo") {
          buf = frustum_buffer.get();
          size = sizeof(FrustumBuffer);
        }
        buffer_infos[index] = {
          .buffer = buf->get(),
          .offset = i * size,
          .range = size,
        };
        write.pBufferInfo = &buffer_infos[index];
      } else if (meta.descriptor_type ==
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        Image* img = nullptr;
        if (meta.name == "shadow_image")
          img = shadow_depth_image.get();
        image_infos[index] = {
          .sampler = img->get_sampler(),
          .imageView = img->get_view(),
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        write.pImageInfo = &image_infos[index];
      }
    }
  }

  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<uint32_t>(write_sets.size()),
                         write_sets.data(),
                         0,
                         nullptr);
}

Renderer::Renderer(const Device& dev,
                   const BlueprintRegistry& registry,
                   const Window& win,
                   BS::priority_thread_pool& p)
  : device(&dev)
  , blueprint_registry(&registry)
  , thread_pool(&p)
{
  create_descriptor_set_layout_from_metadata();

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

    auto result = Material::create(*device,
                                   blueprint_registry->get("main_geometry"),
                                   renderer_descriptor_set_layout);
    if (result.has_value()) {
      geometry_material = std::move(result.value());
    } else {
      assert(false && "Failed to create main geometry material.");
    }
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

    auto result = Material::create(*device,
                                   blueprint_registry->get("colour_correction"),
                                   renderer_descriptor_set_layout);
    if (result.has_value()) {
      colour_corrected_material = std::move(result.value());
    } else {
      assert(false && "Failed to create main geometry material.");
    }

    colour_corrected_material->upload("input_image", geometry_image.get());
  }

  {
    auto result = Material::create(*device,
                                   blueprint_registry->get("z_prepass"),
                                   renderer_descriptor_set_layout);
    if (result.has_value()) {
      z_prepass_material = std::move(result.value());
    } else {
      assert(false && "Failed to create z prepass material.");
    }
  }

  {
    auto result = Material::create(
      *device, blueprint_registry->get("line"), renderer_descriptor_set_layout);
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
      std::span<std::byte>{ bytes.get(), instance_size_bytes });
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
      std::span<std::byte>{ bytes.get(), instance_size_bytes });
  }

  {
    auto result = Material::create(*device,
                                   blueprint_registry->get("gizmo"),
                                   renderer_descriptor_set_layout);
    if (result.has_value()) {
      gizmo_material = std::move(result.value());
    } else {
      assert(false && "Failed to create gizmo material.");
    }

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

    auto result = Material::create(*device,
                                   blueprint_registry->get("shadow"),
                                   renderer_descriptor_set_layout);
    if (result.has_value()) {
      shadow_material = std::move(result.value());
    } else {
      assert(false && "Failed to create shadow material.");
    }
  }

  {
    auto result = Material::create(*device,
                                   blueprint_registry->get("compute_culling"),
                                   renderer_descriptor_set_layout);

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
        std::span<std::byte>{ bytes.get(), instance_size_bytes });
    }

    cull_instances_compute_material->upload("InstanceInput",
                                            instance_vertex_buffer.get());
    cull_instances_compute_material->upload(
      "InstanceOutput", culled_instance_vertex_buffer.get());
    cull_instances_compute_material->upload("CounterBuffer",
                                            culled_instance_count_buffer.get());
  }

  finalize_renderer_descriptor_sets();
}

auto
Renderer::get_renderer_descriptor_set_layout(Badge<AssetReloader>) const
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
}

Renderer::~Renderer()
{
  destroy();
}

auto
Renderer::submit(const DrawCommand& cmd, const glm::mat4& transform) -> void
{
  draw_commands[cmd].emplace_back(transform);
  if (cmd.casts_shadows) {
    shadow_draw_commands[cmd].emplace_back(transform);
  }
}

auto
Renderer::submit_lines(const glm::vec3& start,
                       const glm::vec3& end,
                       float width,
                       const glm::vec4& color) -> void
{
  std::uint32_t packed_color =
    (static_cast<std::uint32_t>(color.a * 255.0f) << 24) |
    (static_cast<std::uint32_t>(color.b * 255.0f) << 16) |
    (static_cast<std::uint32_t>(color.g * 255.0f) << 8) |
    (static_cast<std::uint32_t>(color.r * 255.0f) << 0);

  line_instances.push_back(LineInstanceData{
    .start = start,
    .width = width,
    .end = end,
    .packed_color = packed_color,
  });
}

static auto
upload_instance_vertex_data(auto& pool,
                            GPUBuffer& buffer,
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

    // Parallel frustum culling
    auto fut = pool.submit_loop(0, jobs.size(), [&](std::size_t i) {
      const auto& [cmd, instances] = jobs[i];
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
    .light_color = light_environment.light_color,
    .ambient_color = light_environment.ambient_color,
  };

  shadow_camera_buffer->upload_with_offset(std::span{ &shadow_data, 1 },
                                           sizeof(ShadowBuffer) * frame_index);
  light_frustum.update(shadow_data.light_vp);
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
  shadow_draw_commands.clear();
}

auto
Renderer::end_frame(std::uint32_t frame_index) -> void
{
  ZoneScoped;

  if (shadow_draw_commands.empty() && draw_commands.empty())
    return;

  Renderer::DrawList flat_shadow_draw_commands;
  Renderer::DrawList flat_draw_commands;
  std::size_t shadow_count{ 0 };

  {
    std::latch uploads_remaining(3);
    thread_pool->detach_task([&] {
      ZoneScopedN("Instance Upload");
      flat_draw_commands =
        upload_instance_vertex_data(*thread_pool,
                                    *instance_vertex_buffer,
                                    draw_commands,
                                    current_frustum,
                                    instance_count_this_frame);
      uploads_remaining.count_down();
    });

    thread_pool->detach_task([&] {
      ZoneScopedN("Shadow Instance Upload");
      flat_shadow_draw_commands =
        upload_instance_vertex_data(*thread_pool,
                                    *instance_shadow_vertex_buffer,
                                    shadow_draw_commands,
                                    light_frustum,
                                    shadow_count);
      uploads_remaining.count_down();
    });

    thread_pool->detach_task([this, &uploads = uploads_remaining] {
      ZoneScopedN("Line instance upload");
      upload_line_instance_data();
      uploads.count_down();
    });

    uploads_remaining.wait();
  }

  run_culling_compute_pass(frame_index);

  command_buffer->begin_frame(frame_index);

  run_shadow_pass(frame_index, flat_shadow_draw_commands);
  run_z_prepass(frame_index, flat_draw_commands);
  run_geometry_pass(frame_index, flat_draw_commands);
  run_line_pass(frame_index);
  run_gizmo_pass(frame_index);

  run_postprocess_passes(frame_index);

  command_buffer->submit_and_end(frame_index,
                                 compute_finished_semaphore.at(frame_index));

  draw_commands.clear();
}

auto
Renderer::resize(std::uint32_t width, std::uint32_t height) -> void
{
  geometry_image->resize(width, height);
  geometry_msaa_image->resize(width, height);
  geometry_depth_image->resize(width, height);
  colour_corrected_image->resize(width, height);

  colour_corrected_material->invalidate(geometry_image.get());
}

auto
Renderer::get_output_image() const -> const Image&
{
  return *colour_corrected_image;
}

#pragma region RenderPasses

auto
Renderer::run_z_prepass(std::uint32_t frame_index, const DrawList& draw_list)
  -> void
{
  ZoneScopedN("Z Prepass");

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
  ZoneScopedN("Geometry pass");

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

  const std::array<VkDeviceSize, 1> offsets = { 0 };
  const std::array<VkBuffer, 1> buffers = { line_instance_buffer->get() };
  vkCmdBindVertexBuffers(cmd, 0, 1, buffers.data(), offsets.data());

  vkCmdDraw(cmd, 4, line_instance_count_this_frame, 0, 0);

  vkCmdEndRendering(cmd);
  CoreUtils::cmd_transition_to_shader_read(cmd, geometry_image->get_image());
  command_buffer->end_timer(frame_index, "line_pass");
}

auto
Renderer::run_gizmo_pass(std::uint32_t frame_index) -> void
{
  ZoneScopedN("Gizmo pass");

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
    .pDepthAttachment = nullptr,
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
  ZoneScopedN("Shadow pass");

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
      instance_shadow_vertex_buffer->get(),
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

  VkClearValue clear_value = { .color = { { 0.F, 0.F, 0.F, 0.F } } };
  VkRenderingAttachmentInfo color_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = colour_corrected_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = clear_value,
  };

  VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { { 0, 0 },
                    { colour_corrected_image->width(),
                      colour_corrected_image->height() } },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(colour_corrected_image->height()),
    .width = static_cast<float>(colour_corrected_image->width()),
    .height = -static_cast<float>(colour_corrected_image->height()),
    .minDepth = 0.f,
    .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto* material = get_material_by_name("colour_correction");
  const auto& pipeline = material->get_pipeline();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

  const auto& descriptor_set = material->prepare_for_rendering(frame_index);

  const std::array descriptor_sets{
    renderer_descriptor_sets[frame_index],
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

#pragma endregion RenderPasses
