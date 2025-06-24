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

#include "assets/manager.hpp"
#include "core/vulkan_util.hpp"
#include "renderer/camera.hpp"
#include "renderer/editor_camera.hpp"
#include "renderer/mesh.hpp"
#include "renderer/techniques/fullscreen_technique.hpp"
#include "renderer/techniques/shadow_gui_technique.hpp"
#include "window/window.hpp"

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
    .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
    .name = "shadow_depth",
  },
};

auto
to_renderpass(const std::string_view name) -> RenderPass
{
  auto lowercase = std::string(name);
  std::ranges::transform(
    lowercase.begin(), lowercase.end(), lowercase.begin(), ::tolower);
  if (lowercase == "main_geometry") {
    return RenderPass::MainGeometry;
  }
  if (lowercase == "shadow") {
    return RenderPass::Shadow;
  }
  if (lowercase == "line") {
    return RenderPass::Line;
  }
  if (lowercase == "z_prepass") {
    return RenderPass::ZPrepass;
  }
  if (lowercase == "colour_correction") {
    return RenderPass::ColourCorrection;
  }
  if (lowercase == "skybox") {
    return RenderPass::Skybox;
  }
  if (lowercase == "cull_prefix_sum_first") {
    return RenderPass::ComputePrefixCullingFirst;
  }
  if (lowercase == "cull_prefix_sum_second") {
    return RenderPass::ComputePrefixCullingSecond;
  }
  if (lowercase == "cull_prefix_sum_distribute") {
    return RenderPass::ComputePrefixCullingDistribute;
  }
  if (lowercase == "cull_scatter") {
    return RenderPass::ComputeCullingScatter;
  }
  if (lowercase == "cull_visibility") {
    return RenderPass::ComputeCullingVisibility;
  }
  if (lowercase == "shadow_gui") {
    return RenderPass::ShadowGUI;
  }

  return RenderPass::Invalid;
}

Renderer::Renderer(const Device& dev,
                   const Window& win,
                   BS::priority_thread_pool& p)
  : device(&dev)
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
  {
    geometry_image = Image::create(dev,
                                   ImageConfiguration{
                                     .extent = win.framebuffer_size(),
                                     .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                     .debug_name = "Main Geometry Image",
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
                      .debug_name = "Main Geometry Image (MSAA)",
                    });
    geometry_depth_image =
      Image::create(dev,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_D32_SFLOAT,
                      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                      .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                      .sample_count = VK_SAMPLE_COUNT_1_BIT,
                      .allow_in_ui = false,
                      .debug_name = "Geometry Depth Image",
                    });
    geometry_depth_msaa_image =
      Image::create(dev,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_D32_SFLOAT,
                      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                      .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                      .sample_count = sample_count,
                      .allow_in_ui = false,
                      .debug_name = "Geometry Depth MSAA Image",
                    });

    if (auto result = Material::create(*device, "main_geometry");
        result.has_value()) {
      geometry_material = std::move(result.value());
    } else {
      assert(false && "Failed to create main geometry material.");
    }
  }

  {
    bloom_pass = std::make_unique<BloomPass>(*device, geometry_image.get(), 3);
  }

  {
    // Environment map
    skybox_image = Image::load_cubemap(*device, "sf.ktx2");
    if (!skybox_image) {
      assert(false && "Failed to load environment map.");
    }

    auto result = Material::create(*device, "skybox");
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
    composite_attachment_texture =
      Image::create(*device,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                      .debug_name = "composite_attachment_texture",
                    });

    if (auto result = Material::create(*device, "composite");
        result.has_value()) {
      composite_attachment_material = std::move(result.value());
    } else {
      assert(false && "Failed to create composite material.");
    }

    composite_attachment_material->upload("skybox_input",
                                          skybox_attachment_texture.get());
    composite_attachment_material->upload("geometry_input",
                                          geometry_image.get());
    composite_attachment_material->upload("bloom_input",
                                          &bloom_pass->get_output_image());
  }

  {
    colour_corrected_image =
      Image::create(*device,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_B8G8R8A8_SRGB,
                      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                      .debug_name = "colour_corrected_image",
                    });

    if (auto result = Material::create(*device, "colour_correction");
        result.has_value()) {
      colour_corrected_material = std::move(result.value());
    } else {
      assert(false && "Failed to create main geometry material.");
    }

    colour_corrected_material->upload("input_image",
                                      composite_attachment_texture.get());
  }

  {
    if (auto result = Material::create(*device, "z_prepass");
        result.has_value()) {
      z_prepass_material = std::move(result.value());
    } else {
      assert(false && "Failed to create z prepass material.");
    }
  }

  {
    if (auto result = Material::create(*device, "line"); result.has_value()) {
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
      std::as_bytes(std::span{ bytes.get(), instance_size_bytes }));
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
      std::as_bytes(std::span{ bytes.get(), instance_size_bytes }));
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
      std::as_bytes(std::span{ bytes.get(), instance_size_bytes }));
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

    if (auto result = Material::create(*device, "shadow"); result.has_value()) {
      shadow_material = std::move(result.value());
    } else {
      assert(false && "Failed to create shadow material.");
    }
  }

  {
    identifier_image =
      Image::create(dev,
                    ImageConfiguration{
                      .extent = win.framebuffer_size(),
                      .format = VK_FORMAT_R32_UINT,
                      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                      .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                      .sample_count = VK_SAMPLE_COUNT_1_BIT,
                      .debug_name = "identifier_image",
                    });

    if (auto result = Material::create(*device, "identifier");
        result.has_value()) {
      identifier_material = std::move(result.value());
    } else {
      assert(false && "Failed to create shadow material.");
    }

    identifier_buffer = GPUBuffer::zero_initialise(
      *device,
      1'000'000 * sizeof(std::uint32_t),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    identifier_material->upload("identifiers", identifier_buffer);
  }

  {

    culled_instance_count_buffer = std::make_unique<GPUBuffer>(
      *device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    static constexpr std::uint32_t zero = 0;
    culled_instance_count_buffer->upload(std::span{ &zero, 1 });

    culled_instance_vertex_buffer = std::make_unique<GPUBuffer>(
      *device,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      true);

    {
      auto&& [bytes, instance_size_bytes] =
        make_bytes<InstanceData, 1'000'000>();
      culled_instance_vertex_buffer->upload(
        std::span{ bytes.get(), instance_size_bytes });
    }
    {

      visibility_buffer =
        GPUBuffer::zero_initialise(*device,
                                   sizeof(std::uint32_t) * 1'000'000,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   true,
                                   "visibility_buffer");

      prefix_sum_buffer =
        GPUBuffer::zero_initialise(*device,
                                   sizeof(std::uint32_t) * 1'000'000,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   true,
                                   "prefix_sum_buffer");

      static constexpr std::size_t max_workgroups = 16384;

      workgroup_sum_buffer =
        GPUBuffer::zero_initialise(*device,
                                   sizeof(std::uint32_t) * max_workgroups,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   true,
                                   "workgroup_sum_buffer");

      workgroup_sum_prefix_buffer =
        GPUBuffer::zero_initialise(*device,
                                   sizeof(std::uint32_t) * max_workgroups,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   true,
                                   "workgroup_sum_prefix_buffer");
    }

    auto result = Material::create(*device, "cull_visibility");
    assert(result.has_value());
    cull_visibility_material = std::move(result.value());

    result = Material::create(*device, "cull_scatter");
    assert(result.has_value());
    cull_scatter_material = std::move(result.value());

    cull_prefix_sum_material_first =
      Material::create(*device, "cull_prefix_sum_first").value();

    cull_prefix_sum_material_second =
      Material::create(*device, "cull_prefix_sum_second").value();

    cull_prefix_sum_material_distribute =
      Material::create(*device, "cull_prefix_sum_distribute").value();

    cull_visibility_material->upload("InstanceInput",
                                     instance_vertex_buffer.get());
    cull_visibility_material->upload("VisibilityOutput",
                                     visibility_buffer.get());

    cull_scatter_material->upload("InstanceInput",
                                  instance_vertex_buffer.get());
    cull_scatter_material->upload("VisibilityInput", visibility_buffer.get());
    cull_scatter_material->upload("PrefixSumInput", prefix_sum_buffer.get());
    cull_scatter_material->upload("InstanceOutput",
                                  culled_instance_vertex_buffer.get());
    cull_scatter_material->upload("Counter",
                                  culled_instance_count_buffer.get());

    cull_prefix_sum_material_first->upload("VisibilityInput",
                                           visibility_buffer.get());
    cull_prefix_sum_material_first->upload("PrefixSumOutput",
                                           prefix_sum_buffer.get());
    cull_prefix_sum_material_first->upload("WorkgroupSums",
                                           workgroup_sum_buffer.get());

    cull_prefix_sum_material_second->upload("WorkgroupSums",
                                            workgroup_sum_buffer.get());
    cull_prefix_sum_material_second->upload("WorkgroupPrefix",
                                            workgroup_sum_prefix_buffer.get());

    cull_prefix_sum_material_distribute->upload("PrefixSumOutput",
                                                prefix_sum_buffer.get());
    cull_prefix_sum_material_distribute->upload(
      "WorkgroupPrefix", workgroup_sum_prefix_buffer.get());
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

  white_texture = Image::create(*device,
                                {
                                  .extent = { 1, 1 },
                                  .format = VK_FORMAT_R8G8B8A8_SRGB,
                                  .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                           VK_IMAGE_USAGE_SAMPLED_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                  .sample_count = VK_SAMPLE_COUNT_1_BIT,
                                  .allow_in_ui = false,
                                  .debug_name = "white_texture",
                                });
  static constexpr std::array<unsigned char, 4> white_pixel = {
    0xff, 0xff, 0xff, 0xff
  };
  white_texture->upload_rgba(white_pixel);

  black_texture = Image::create(*device,
                                {
                                  .extent = { 1, 1 },
                                  .format = VK_FORMAT_R8G8B8A8_SRGB,
                                  .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                           VK_IMAGE_USAGE_SAMPLED_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                  .sample_count = VK_SAMPLE_COUNT_1_BIT,
                                  .allow_in_ui = false,
                                  .debug_name = "black_texture",
                                });

  static constexpr std::array<unsigned char, 4> black_pixel = {
    0x0, 0x0, 0x0, 0x0
  };
  black_texture->upload_rgba(black_pixel);

  VkSemaphoreCreateInfo semaphore_create_info{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
  };
  for (auto& sema : geometry_complete_semaphores) {
    vkCreateSemaphore(
      device->get_device(), &semaphore_create_info, nullptr, &sema);
  }

  for (auto& sema : bloom_complete_semaphores) {
    vkCreateSemaphore(
      device->get_device(), &semaphore_create_info, nullptr, &sema);
  }

  auto&& [tech, could] = techniques.try_emplace("shadow_gui");
  tech->second = FullscreenTechniqueFactory::create(
    "shadow_gui", *device, *descriptor_set_manager);

  string_hash_map<const Image*> image_technique_map;
  string_hash_map<const GPUBuffer*> buffer_technique_map;
  image_technique_map["shadow_depth_image"] = shadow_depth_image.get();

  for (auto&& [k, t] : techniques) {
    t->initialise(*this, image_technique_map, buffer_technique_map);
  }
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
  for (auto& sema : geometry_complete_semaphores) {
    vkDestroySemaphore(device->get_device(), sema, nullptr);
  }
  for (auto& sema : bloom_complete_semaphores) {
    vkDestroySemaphore(device->get_device(), sema, nullptr);
  }

  white_texture.reset();
  black_texture.reset();
}

Renderer::~Renderer()
{
  destroy();
}

auto
Renderer::submit(const RendererSubmit& cmd,
                 const glm::mat4& transform,
                 const std::uint32_t optional_identifier) -> void
{
  if (cmd.mesh == nullptr) {
    return;
  }

  const bool has_optional_identifier = optional_identifier != 0;

  for (auto& submesh : cmd.mesh->get_submeshes()) {
    auto command = DrawCommand{
      .mesh = cmd.mesh,
      .override_material = cmd.override_material,
      .submesh_index = cmd.mesh->get_submesh_index(submesh),
    };
    draw_commands[command].emplace_back(transform);
    if (has_optional_identifier) {
      identifiers[command].emplace_back(optional_identifier);
    }
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

auto
Renderer::submit_aabb(const glm::vec3& min,
                      const glm::vec3& max,
                      const glm::vec4& color,
                      const float width) -> void
{
  const std::array<glm::vec3, 8> corners = {
    glm::vec3{ min.x, min.y, min.z }, { max.x, min.y, min.z },
    { max.x, max.y, min.z },          { min.x, max.y, min.z },
    { min.x, min.y, max.z },          { max.x, min.y, max.z },
    { max.x, max.y, max.z },          { min.x, max.y, max.z },
  };

  constexpr std::array<std::pair<int, int>, 12> edges = {
    std::pair{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 },
    { 6, 7 },          { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
  };

  for (const auto& [start, end] : edges)
    submit_lines(corners[start], corners[end], width, color);
}

static auto
cpu_mt_cull_and_upload_to_gpu(BS::priority_thread_pool& pool,
                              GPUBuffer& buffer,
                              const DrawCommandMap& draw_commands,
                              const auto& frustum,
                              std::integral auto& instance_count_this_frame)
  -> DrawList
{
  using Submit = DrawInstanceSubmit;
  using WorkItem = std::pair<const DrawCommand*, InstanceData>;

  constexpr std::size_t bucket_count = 8;
  std::array<std::vector<Submit>, bucket_count> buckets;
  std::array<std::mutex, bucket_count> locks;

  std::vector<WorkItem> all_instances;
  for (const auto& [cmd, instances] : draw_commands)
    for (const auto& instance : instances)
      all_instances.emplace_back(&cmd, instance);

  if (all_instances.empty()) {
    instance_count_this_frame = 0;
    return {};
  }

  auto fut = pool.submit_loop(0, all_instances.size(), [&](std::size_t i) {
    const auto& [cmd, instance] = all_instances[i];
    const glm::vec3 center_ws = glm::vec3(instance.transform[3]);
    const float radius_ws = glm::length(glm::vec3(instance.transform[0]));

    if (frustum.intersects(center_ws, radius_ws)) {
      const std::size_t bucket_index =
        reinterpret_cast<std::uintptr_t>(cmd) % bucket_count;
      std::scoped_lock lock(locks[bucket_index]);
      buckets[bucket_index].emplace_back(Submit{ cmd, instance });
    }
  });

  fut.wait();

  std::vector<Submit> merged;
  for (auto& bucket : buckets)
    merged.insert(merged.end(), bucket.begin(), bucket.end());

  instance_count_this_frame = static_cast<std::uint32_t>(merged.size());
  if (merged.empty())
    return {};

  std::vector<InstanceData> instance_data(merged.size());
  for (std::size_t i = 0; i < merged.size(); ++i)
    instance_data[i] = merged[i].data;

  buffer.upload(std::span(instance_data));

  std::unordered_map<DrawCommand,
                     std::tuple<std::uint32_t, std::uint32_t>,
                     DrawCommandHasher>
    draw_map;

  for (std::size_t i = 0; i < merged.size(); ++i) {
    const auto* cmd = merged[i].cmd;
    auto& [start, count] = draw_map[*cmd];
    if (count == 0)
      start = static_cast<std::uint32_t>(i);
    ++count;
  }

  DrawList flat;
  flat.reserve(draw_map.size());
  for (const auto& [cmd, range] : draw_map)
    flat.emplace_back(cmd, std::get<0>(range), std::get<1>(range));

  return flat;
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
Renderer::update_shadow_buffers(const std::uint32_t frame_index) -> void
{
  constexpr glm::vec3 up{ camera_constants::WORLD_UP };

  glm::mat4 view;
  switch (light_environment.view_mode) {
    case ShadowViewMode::LookAtRH:
      view = glm::lookAtRH(
        light_environment.light_position, light_environment.target, up);
      break;
    case ShadowViewMode::LookAtLH:
      view = glm::lookAtLH(
        light_environment.light_position, light_environment.target, up);
      break;
  }

  const float s = light_environment.ortho_size;
  const float n = light_environment.near_plane;
  const float f = light_environment.far_plane;

  glm::mat4 proj;
  switch (light_environment.projection_mode) {
    case ShadowProjectionMode::OrthoRH_ZO:
      proj = glm::orthoRH_ZO(-s, s, -s, s, n, f);
      break;
    case ShadowProjectionMode::OrthoRH_NO:
      proj = glm::orthoRH_NO(-s, s, -s, s, n, f);
      break;
    case ShadowProjectionMode::OrthoLH_ZO:
      proj = glm::orthoLH_ZO(-s, s, -s, s, n, f);
      break;
    case ShadowProjectionMode::OrthoLH_NO:
      proj = glm::orthoLH_NO(-s, s, -s, s, n, f);
      break;
  }

  const glm::mat4 vp = proj * view;

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

void
Renderer::update_identifiers()
{
  std::vector<std::uint32_t> ids;
  static std::uint64_t ids_count = 5;
  ids.reserve(ids_count);

  for (auto& v : identifiers | std::views::values) {
    ids.append_range(v);
  }

  if (ids_count != ids.size())
    ids_count = ids.size();

  identifier_buffer->upload(std::span(ids));
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
Renderer::begin_frame(const std::uint32_t frame_index, const VP& matrices)
  -> void
{
  const auto vp = matrices.projection * matrices.view;
  const auto position = matrices.view[3];
  update_uniform_buffers(frame_index,
                         matrices.view,
                         matrices.projection,
                         matrices.inverse_projection,
                         position);
  update_shadow_buffers(frame_index);
  update_frustum(vp);
  update_identifiers();
  draw_commands.clear();
  shadow_draw_commands.clear();
  identifiers.clear();
}

static constexpr auto run_technique_passes =
  [](const auto& techniques, const auto& cmd, auto fm) {
    Util::Vulkan::cmd_begin_debug_label(
      cmd.get(fm), "Technique passes", { 0.9F, 0.9F, 0.1F, 1.0F });
    std::ranges::for_each(techniques, [c = &cmd, fm](const auto& t) {
      auto&& [k, v] = t;
      v->perform(*c, fm);
    });
    Util::Vulkan::cmd_end_debug_label(cmd.get(fm));
  };

auto
Renderer::end_frame(const std::uint32_t frame_index) -> void
{
  ZoneScopedN("End frame");

  if (shadow_draw_commands.empty() && draw_commands.empty())
    return;

  DrawList flat_shadow_draw_commands;
  DrawList flat_draw_commands;
  std::size_t shadow_count{ 0 };

  std::latch uploads_remaining(3);

  constexpr std::size_t culling_threshold = 5000;
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
      ZoneScopedN("Geom Culling");
      return DrawListManager::should_perform_culling(draw_commands,
                                                     culling_threshold);
    },
    [&] {
      ZoneScopedN("Shadow Culling");
      return DrawListManager::should_perform_culling(shadow_draw_commands,
                                                     culling_threshold);
    });

  if (should_geom_cull) {
    thread_pool->detach_task([this, &flat_draw_commands, &uploads_remaining] {
      ZoneScopedN("Instance Upload");
      flat_draw_commands =
        cpu_mt_cull_and_upload_to_gpu(*thread_pool,
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
          cpu_mt_cull_and_upload_to_gpu(*thread_pool,
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

  {
    ZoneScopedN("Performance uploads");
    uploads_remaining.wait();
  }

  {
    ZoneScopedN("Begin command buffer");
    command_buffer->begin_frame(frame_index);
  }

  run_skybox_pass(frame_index);

  run_shadow_pass(frame_index, flat_shadow_draw_commands);
  run_z_prepass(frame_index, flat_draw_commands);
  run_geometry_pass(frame_index, flat_draw_commands);

#if IS_DEBUG
  run_identifier_pass(frame_index, flat_draw_commands);
#endif

  // Add image barrier to prepare geometry_image for compute shader read
  {
    ZoneScopedN("Geometry to Compute Barrier");
    VkImageMemoryBarrier geometry_barrier{};
    geometry_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    geometry_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    geometry_barrier.dstAccessMask = 0;
    geometry_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    geometry_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    geometry_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    geometry_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    geometry_barrier.image = geometry_image->get_image();
    geometry_barrier.subresourceRange = {
      VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
    };

    vkCmdPipelineBarrier(command_buffer->get(frame_index),
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &geometry_barrier);
  }

  // Submit graphics work and signal semaphore
  command_buffer->submit_and_end(
    frame_index, VK_NULL_HANDLE, geometry_complete_semaphores.at(frame_index));

  // Begin compute work
  compute_command_buffer->begin_frame(frame_index);

  run_bloom_pass(frame_index);

  compute_command_buffer->submit_and_end(
    frame_index,
    geometry_complete_semaphores.at(frame_index),
    bloom_complete_semaphores.at(frame_index));

  // Begin final graphics work
  command_buffer->begin_frame_persist_query_pools(frame_index);

  run_composite_pass(frame_index);
  run_postprocess_passes(frame_index);

  run_technique_passes(techniques, *command_buffer, frame_index);

  command_buffer->submit_and_end(
    frame_index, bloom_complete_semaphores.at(frame_index), VK_NULL_HANDLE);
}

auto
Renderer::resize(const std::uint32_t width, const std::uint32_t height) -> void
{
  geometry_image->resize(width, height);
  geometry_msaa_image->resize(width, height);
  geometry_depth_image->resize(width, height);
  geometry_depth_msaa_image->resize(width, height);
  skybox_attachment_texture->resize(width, height);
  composite_attachment_texture->resize(width, height);
  colour_corrected_image->resize(width, height);

  bloom_pass->resize(width, height);

  if (skybox_material)
    skybox_material->invalidate(skybox_attachment_texture.get());

  if (composite_attachment_material) {
    composite_attachment_material->invalidate(skybox_attachment_texture.get());
    composite_attachment_material->invalidate(geometry_image.get());
    composite_attachment_material->invalidate(&bloom_pass->get_output_image());
  }

  if (colour_corrected_material)
    colour_corrected_material->invalidate(composite_attachment_texture.get());
}

auto
Renderer::get_output_image() const -> const Image&
{
  return *colour_corrected_image;
}

auto
Renderer::update_camera(const EditorCamera& camera) -> void
{
  update_frustum(camera.get_projection() * camera.get_view());

  camera_environment = {
    .view = camera.get_view(),
    .projection = camera.get_projection(),
    .inverse_projection = camera.get_inverse_projection(),
    .z_near = camera.get_projection_config().znear,
    .z_far = camera.get_projection_config().zfar,
  };
}

#pragma region RenderPasses

auto
Renderer::run_skybox_pass(std::uint32_t frame_index) -> void
{
  ZoneScopedN("Skybox pass");

  command_buffer->begin_timer(frame_index, "skybox_pass");

  const VkCommandBuffer& cmd = command_buffer->get(frame_index);
  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Skybox", { 0.9F, 0.1F, 0.1F, 1.0F });

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

  constexpr VkClearValue clear_value = { .color = { { 0.f, 0.f, 0.f, 0.f } } };
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

  if (const auto mesh_expected =
        MeshCache::the().get_mesh<MeshType::CubeOnlyPosition>();
      mesh_expected.has_value()) {
    const auto& mesh = mesh_expected.value();
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

  Util::Vulkan::cmd_end_debug_label(cmd);
}

auto
bind_sets(auto cmd, auto layout, std::ranges::contiguous_range auto sets)
{
  auto valid = sets | std::views::filter(
                        [](const auto& v) { return v != VK_NULL_HANDLE; });
  for (auto& v : valid) {
    vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &v, 0, nullptr);
  }
}

auto
Renderer::run_z_prepass(std::uint32_t frame_index, const DrawList& draw_list)
  -> void
{
  ZoneScopedN("Z Prepass");

  command_buffer->begin_timer(frame_index, "z_prepass");

  const VkCommandBuffer& cmd = command_buffer->get(frame_index);
  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Z Prepass", { 0.1F, 0.9F, 0.1F, 1.0F });

  CoreUtils::cmd_transition_to_depth_attachment(
    cmd, geometry_depth_msaa_image->get_image());

  constexpr VkClearValue depth_clear = { .depthStencil = { 0.0f, 0 } };
  VkRenderingAttachmentInfo depth_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = geometry_depth_msaa_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
    .resolveImageView = geometry_depth_image->get_view(),
    .resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
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

  const std::array sets = {
    descriptor_set_manager->get_set(frame_index),
    z_prepass_material->prepare_for_rendering(frame_index),
  };
  bind_sets(cmd, z_prepass_pipeline.layout, sets);
  for (const auto& [cmd_info, offset, instance_count] : draw_list) {
    const auto* submesh = cmd_info.mesh->get_submesh(cmd_info.submesh_index);
    if (!submesh)
      continue;

    const auto& vb = cmd_info.mesh->get_vertex_buffer();
    const auto& ib = cmd_info.mesh->get_index_buffer();

    const VkDeviceSize vb_offset = submesh->vertex_offset * sizeof(Vertex);
    const std::array vertex_buffers = { vb->get(),
                                        instance_vertex_buffer->get() };
    const std::array offsets = { vb_offset, 0ULL };

    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(cmd, ib->get(), 0, ib->get_index_type());

    vkCmdDrawIndexed(cmd,
                     submesh->index_count,
                     instance_count,
                     submesh->index_offset,
                     0,
                     offset);
  }

  vkCmdEndRendering(cmd);
  command_buffer->end_timer(frame_index, "z_prepass");

  Util::Vulkan::cmd_end_debug_label(cmd);
}

auto
Renderer::run_geometry_pass(std::uint32_t frame_index,
                            const DrawList& draw_list) -> void
{
  ZoneScopedN("Geometry pass");

  command_buffer->begin_timer(frame_index, "geometry_pass");

  const VkCommandBuffer& cmd = command_buffer->get(frame_index);
  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Geometry Pass", { 0.5F, 0.5F, 0.0F, 1.0F });
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
    .imageView = geometry_depth_msaa_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = VK_NULL_HANDLE,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .clearValue = {},
  };

  const std::array colour_attachments = { color_attachment };
  const VkRenderingInfo render_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { .offset = { 0, 0 },
                    .extent = { geometry_image->width(),
                                geometry_image->height() } },
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount =
      static_cast<std::uint32_t>(colour_attachments.size()),
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

  // The pipeline should still come from the geometry main material.
  auto& pipeline = geometry_material->get_pipeline();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

  for (auto&& [cmd_info, offset, instance_count] : draw_list) {
    const auto* submesh = cmd_info.mesh->get_submesh(cmd_info.submesh_index);
    if (!submesh)
      continue;

    const auto& vertex_buffer = cmd_info.mesh->get_vertex_buffer();
    const auto& index_buffer = cmd_info.mesh->get_index_buffer();
    const auto& submesh_material =
      cmd_info.mesh->get_material_by_submesh_index(cmd_info.submesh_index);

    Material& material =
      cmd_info.override_material.is_valid()
        ? *Assets::Manager::the().get(cmd_info.override_material)
        : *submesh_material;

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

    const VkDeviceSize vertex_offset_bytes =
      submesh->vertex_offset * sizeof(Vertex);
    const std::array vertex_buffers = {
      vertex_buffer->get(),
      instance_vertex_buffer->get(),
    };
    const std::array offsets = { vertex_offset_bytes, 0ULL };

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
                     submesh->index_count,
                     instance_count,
                     submesh->index_offset,
                     0,
                     offset);
  }

  if (line_instance_count_this_frame > 0) {
    Util::Vulkan::cmd_begin_debug_label(
      cmd, "Line Pass (Geometry)", { 0.5F, 0.5F, 0.0F, 1.0F });

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

    constexpr std::array line_offsets = { VkDeviceSize{ 0 } };
    const std::array line_buffers = { line_instance_buffer->get() };
    vkCmdBindVertexBuffers(cmd, 0, 1, line_buffers.data(), line_offsets.data());

    vkCmdDraw(cmd, 4, line_instance_count_this_frame, 0, 0);
    Util::Vulkan::cmd_end_debug_label(cmd);
  }

  vkCmdEndRendering(cmd);
  // CoreUtils::cmd_transition_to_shader_read(cmd, geometry_image->get_image());

  command_buffer->end_timer(frame_index, "geometry_pass");

  Util::Vulkan::cmd_end_debug_label(cmd);
}

auto
Renderer::run_bloom_pass(uint32_t frame_index) -> void
{
  compute_command_buffer->begin_timer(frame_index, "bloom_pass");

  const VkCommandBuffer& cmd = compute_command_buffer->get(frame_index);
  bloom_pass->update_source(geometry_image.get());
  bloom_pass->prepare(frame_index);
  bloom_pass->record(cmd, *descriptor_set_manager, frame_index);

  compute_command_buffer->end_timer(frame_index, "bloom_pass");
}

auto
Renderer::run_identifier_pass(const std::uint32_t frame_index,
                              const DrawList& draw_list) -> void
{
  ZoneScopedN("Identifier pass");

  command_buffer->begin_timer(frame_index, "identifier_pass");

  const VkCommandBuffer& cmd = command_buffer->get(frame_index);
  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Identifier Pass", { 0.3F, 0.0F, 0.9F, 1.0F });

  CoreUtils::cmd_transition_to_color_attachment(cmd,
                                                identifier_image->get_image());

  constexpr VkClearValue identifier_colour = { .color = { 0.f } };

  VkRenderingAttachmentInfo colour_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = identifier_image->get_view(),
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = VK_NULL_HANDLE,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = identifier_colour,
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
    .clearValue = {},
  };

  const VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { { 0, 0 },
                    { identifier_image->width(), identifier_image->height() } },
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colour_attachment,
    .pDepthAttachment = &depth_attachment,
    .pStencilAttachment = nullptr,
  };

  vkCmdBeginRendering(cmd, &rendering_info);

  const VkViewport viewport = {
    .x = 0.f,
    .y = static_cast<float>(identifier_image->height()),
    .width = static_cast<float>(identifier_image->width()),
    .height = -static_cast<float>(identifier_image->height()),
    .minDepth = 1.f,
    .maxDepth = 0.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &rendering_info.renderArea);

  auto& identifier_pipeline = identifier_material->get_pipeline();
  vkCmdBindPipeline(
    cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, identifier_pipeline.pipeline);

  const std::vector sets{
    descriptor_set_manager->get_set(frame_index),
    identifier_material->prepare_for_rendering(frame_index),
  };
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          identifier_pipeline.layout,
                          0,
                          static_cast<std::uint32_t>(sets.size()),
                          sets.data(),
                          0,
                          nullptr);

  for (const auto& [cmd_info, offset, instance_count] : draw_list) {
    const auto* submesh = cmd_info.mesh->get_submesh(cmd_info.submesh_index);
    if (!submesh)
      continue;

    const auto& vb = cmd_info.mesh->get_vertex_buffer();
    const auto& ib = cmd_info.mesh->get_index_buffer();

    const VkDeviceSize vb_offset = submesh->vertex_offset * sizeof(Vertex);
    const std::array vertex_buffers = { vb->get(),
                                        instance_vertex_buffer->get() };
    const std::array offsets = { vb_offset, 0ULL };

    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(cmd, ib->get(), 0, ib->get_index_type());

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            identifier_pipeline.layout,
                            0,
                            1,
                            &descriptor_set_manager->get_set(frame_index),
                            0,
                            nullptr);

    vkCmdDrawIndexed(cmd,
                     submesh->index_count,
                     instance_count,
                     submesh->index_offset,
                     0,
                     offset);
  }

  vkCmdEndRendering(cmd);
  CoreUtils::cmd_transition_to_shader_read(cmd, identifier_image->get_image());

  command_buffer->end_timer(frame_index, "identifier_pass");

  Util::Vulkan::cmd_end_debug_label(cmd);
}

auto
Renderer::run_culling_compute_pass(std::uint32_t frame_index) -> void
{
  ZoneScopedN("Compute pass");

  compute_command_buffer->begin_timer(frame_index, "gpu_culling");

  const auto cmd = compute_command_buffer->get(frame_index);

  static constexpr std::uint32_t zero = 0;
  culled_instance_count_buffer->upload(std::span{ &zero, 1 });

  constexpr std::uint32_t local_size = 64;
  const std::uint32_t num_instances = instance_count_this_frame;
  const std::uint32_t group_count =
    (num_instances + local_size - 1) / local_size;

  auto dispatch = [&](Material& mat,
                      const std::string_view label,
                      const std::uint32_t groups,
                      const bool insert_barrier) {
    compute_command_buffer->begin_timer(frame_index, label);

    const auto& pipeline = mat.get_pipeline();
    const std::array sets = {
      descriptor_set_manager->get_set(frame_index),
      mat.prepare_for_rendering(frame_index),
    };

    vkCmdBindPipeline(cmd, pipeline.bind_point, pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd,
                            pipeline.bind_point,
                            pipeline.layout,
                            0,
                            static_cast<std::uint32_t>(sets.size()),
                            sets.data(),
                            0,
                            nullptr);
    vkCmdDispatch(cmd, groups, 1, 1);
    compute_command_buffer->end_timer(frame_index, label);

    if (insert_barrier) {
      vkCmdPipelineBarrier(cmd,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           0,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           0,
                           nullptr);
    }
  };

  {
    ZoneScopedN("Visibility pass");
    dispatch(*cull_visibility_material, "visibility_pass", group_count, true);
  }
  {
    ZoneScopedN("Prefix sum pass 1");
    dispatch(
      *cull_prefix_sum_material_first, "prefix_sum_pass_1", group_count, true);
  }
  {
    ZoneScopedN("Prefix sum pass 2");
    dispatch(*cull_prefix_sum_material_second, "prefix_sum_pass_2", 1, true);
  }
  {
    ZoneScopedN("Distribute pass");
    dispatch(*cull_prefix_sum_material_distribute,
             "prefix_sum_distribute",
             group_count,
             true);
  }
  {
    ZoneScopedN("Scatter pass");
    dispatch(*cull_scatter_material, "scatter_pass", group_count, false);
  }
  compute_command_buffer->end_timer(frame_index, "gpu_culling");

  std::uint32_t culled_count = 0;
  if (!culled_instance_count_buffer->read_into_with_offset(culled_count, 0)) {
    Logger::log_error("Failed to read culled instance count");
  }
}

auto
Renderer::run_shadow_pass(std::uint32_t frame_index, const DrawList& draw_list)
  -> void
{
  ZoneScopedN("Shadow pass");

  command_buffer->begin_timer(frame_index, "shadow_pass");

  const VkCommandBuffer& cmd = command_buffer->get(frame_index);
  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Shadow Pass", { 0.3F, 0.0F, 0.9F, 1.0F });

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
  for (const auto& [cmd_info, offset, instance_count] : draw_list) {
    const auto* submesh = cmd_info.mesh->get_submesh(cmd_info.submesh_index);
    if (!submesh)
      continue;

    const auto& vb = cmd_info.mesh->get_vertex_buffer();
    const auto& ib = cmd_info.mesh->get_index_buffer();

    const VkDeviceSize vb_offset = submesh->vertex_offset * sizeof(Vertex);
    const std::array vertex_buffers = { vb->get(),
                                        instance_vertex_buffer->get() };
    const std::array offsets = { vb_offset, 0ULL };

    vkCmdBindVertexBuffers(cmd,
                           0,
                           static_cast<std::uint32_t>(vertex_buffers.size()),
                           vertex_buffers.data(),
                           offsets.data());
    vkCmdBindIndexBuffer(cmd, ib->get(), 0, ib->get_index_type());

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            shadow_pipeline.layout,
                            0,
                            1,
                            &descriptor_set_manager->get_set(frame_index),
                            0,
                            nullptr);

    vkCmdDrawIndexed(cmd,
                     submesh->index_count,
                     instance_count,
                     submesh->index_offset,
                     0,
                     offset);
  }

  vkCmdEndRendering(cmd);
  CoreUtils::cmd_transition_depth_to_shader_read(
    cmd, shadow_depth_image->get_image());

  command_buffer->end_timer(frame_index, "shadow_pass");

  Util::Vulkan::cmd_end_debug_label(cmd);
}

auto
Renderer::run_colour_correction_pass(std::uint32_t frame_index) -> void
{
  ZoneScopedN("Colour correction pass");

  command_buffer->begin_timer(frame_index, "colour_correction_pass");

  const auto cmd = command_buffer->get(frame_index);

  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Colour Correction (PP)", { 0.5F, 0.9F, 0.1F, 1.0F });

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

  Util::Vulkan::cmd_end_debug_label(cmd);
}

auto
Renderer::run_composite_pass(const std::uint32_t frame_index) -> void
{
  ZoneScopedN("Composite pass");

  command_buffer->begin_timer(frame_index, "composite_pass");
  const VkCommandBuffer& cmd = command_buffer->get(frame_index);
  Util::Vulkan::cmd_begin_debug_label(
    cmd, "Composite (PP)", { 0.1F, 0.9F, 0.1F, 1.0F });
  CoreUtils::cmd_transition_to_color_attachment(
    cmd, composite_attachment_texture->get_image());

  constexpr VkClearValue clear_value = { .color = { { 0.F, 0.F, 0.F, 0.F } } };
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
  Util::Vulkan::cmd_end_debug_label(cmd);
}

#pragma endregion RenderPasses
