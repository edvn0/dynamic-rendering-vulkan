#pragma once

#include "mesh.hpp"

#include <concepts>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "core/forward.hpp"

#include "core/command_buffer.hpp"
#include "core/config.hpp"
#include "core/device.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image.hpp"
#include "passes/bloom_pass.hpp"
#include "pipeline/blueprint_registry.hpp"
#include "renderer/draw_command.hpp"
#include "renderer/frustum.hpp"
#include "renderer/material.hpp"
#include "techniques/fullscreen_technique.hpp"
#include "techniques/shadow_gui_technique.hpp"

#include <BS_thread_pool.hpp>

enum class RenderPass : std::uint8_t
{
  Invalid,
  MainGeometry,
  Shadow,
  Line,
  ZPrepass,
  ColourCorrection,
  ComputePrefixCullingFirst,
  ComputePrefixCullingSecond,
  ComputePrefixCullingDistribute,
  ComputeCullingScatter,
  ComputeCullingVisibility,
  Skybox,
  ShadowGUI,
};
auto
to_renderpass(std::string_view name) -> RenderPass;

struct LineInstanceData
{
  glm::vec3 start;
  float width;
  glm::vec3 end;
  std::uint32_t packed_color;
};
static_assert(sizeof(LineInstanceData) == 32,
              "LineInstanceData must be 32 bytes.");

class Renderer
{
public:
  Renderer(const Device&, const Window&, BS::priority_thread_pool&);
  ~Renderer();

  auto submit(const RendererSubmit&,
              const glm::mat4& = glm::mat4{ 1.0F },
              std::uint32_t optional_identifier = 0) -> void;
  auto submit_lines(const glm::vec3&, const glm::vec3&, float, const glm::vec4&)
    -> void;
  auto submit_aabb(const glm::vec3& min,
                   const glm::vec3& max,
                   const glm::vec4& color = { 1.f, 1.f, 0.f, 1.f },
                   float width = 1.f) -> void;
  auto submit_aabb(const AABB& aabb,
                   const glm::vec4& color = { 1.f, 1.f, 0.f, 1.f },
                   float width = 1.f)
  {
    submit_aabb(aabb.min(), aabb.max(), color, width);
  }

  struct VP
  {
    const glm::mat4& projection;
    const glm::mat4& inverse_projection;
    const glm::mat4& view;
  };
  auto begin_frame(std::uint32_t, const VP&) -> void;
  auto end_frame(std::uint32_t) -> void;
  auto resize(std::uint32_t, std::uint32_t) -> void;
  [[nodiscard]] auto get_output_image() const -> const Image&;
  [[nodiscard]] auto get_shadow_image() const -> const Image&
  {
    auto* shadow_mapped = techniques.at("shadow_gui").get();
    if (const auto* p = dynamic_cast<ShadowGUITechnique*>(shadow_mapped)) {
      return p->get_output();
    }
    throw;
  }
  [[nodiscard]] auto get_command_buffer() const -> CommandBuffer&
  {
    return *command_buffer;
  }
  [[nodiscard]] auto get_compute_command_buffer() const -> CommandBuffer&
  {
    return *compute_command_buffer;
  }
  auto update_camera(const EditorCamera& camera) -> void;
  auto update_frustum(const glm::mat4& vp) -> void
  {
    camera_frustum.update(vp);
  }

  auto get_light_environment() -> auto& { return light_environment; }
  [[nodiscard]] auto get_light_environment() const -> const auto&
  {
    return light_environment;
  }
  auto get_camera_environment() -> auto& { return camera_environment; }
  [[nodiscard]] auto get_camera_environment() const -> const auto&
  {
    return camera_environment;
  }
  [[nodiscard]] auto get_material_by_name(const std::string& name) const
    -> Material*
  {
    switch (to_renderpass(name)) {
      using enum RenderPass;
      case Invalid:
        return nullptr;
      case MainGeometry:
        return geometry_material.get();
      case Shadow:
        return shadow_material.get();
      case Line:
        return line_material.get();
      case ZPrepass:
        return z_prepass_material.get();
      case ColourCorrection:
        return colour_corrected_material.get();
      case Skybox:
        return skybox_material.get();
      case ComputePrefixCullingFirst:
        return cull_prefix_sum_material_first.get();
      case ComputePrefixCullingSecond:
        return cull_prefix_sum_material_second.get();
      case ComputePrefixCullingDistribute:
        return cull_prefix_sum_material_distribute.get();
      case ComputeCullingScatter:
        return cull_scatter_material.get();
      case ComputeCullingVisibility:
        return cull_visibility_material.get();
      case ShadowGUI:
        return techniques.at("shadow_gui")->get_material();
      default:
        assert(false && "Unknown render pass name");
        return nullptr;
    }
  }
  [[nodiscard]] auto get_renderer_descriptor_set_layout(
    Badge<AssetReloader>) const -> VkDescriptorSetLayout;
  static auto get_white_texture() { return white_texture.get(); }
  static auto get_black_texture() { return black_texture.get(); }

private:
  const Device* device{ nullptr };
  BS::priority_thread_pool* thread_pool{ nullptr };
  std::unique_ptr<DescriptorSetManager> descriptor_set_manager;

  frame_array<VkSemaphore> geometry_complete_semaphores{};
  frame_array<VkSemaphore> bloom_complete_semaphores{};

  Frustum camera_frustum;
  Frustum light_frustum;
  LightEnvironment light_environment;
  CameraEnvironment camera_environment{};

  string_hash_map<Assets::Pointer<IFullscreenTechnique>> techniques;

  std::unique_ptr<CommandBuffer> command_buffer;
  std::unique_ptr<GPUBuffer> instance_vertex_buffer;
  std::unique_ptr<GPUBuffer> instance_shadow_vertex_buffer;

  std::unique_ptr<CommandBuffer> compute_command_buffer;

  Assets::Pointer<Image> geometry_image;
  Assets::Pointer<Image> geometry_msaa_image;
  Assets::Pointer<Image> geometry_depth_image;
  Assets::Pointer<Image> geometry_depth_msaa_image;
  Assets::Pointer<Material> z_prepass_material;
  Assets::Pointer<Material> geometry_material;
  Assets::Pointer<Material> geometry_wireframe_material;

  Assets::Pointer<Material> skybox_material;
  Assets::Pointer<Image> skybox_image;
  Assets::Pointer<Image> skybox_attachment_texture;

  Assets::Pointer<Image> colour_corrected_image;
  Assets::Pointer<Material> colour_corrected_material;

  Assets::Pointer<Image> composite_attachment_texture;
  Assets::Pointer<Material> composite_attachment_material;

  Assets::Pointer<Image> shadow_depth_image;
  Assets::Pointer<Material> shadow_material;

  Assets::Pointer<Image> identifier_image;
  Assets::Pointer<Material> identifier_material;

  Assets::Pointer<Material> line_material;

  std::uint32_t instance_count_this_frame{ 0 };
  std::unique_ptr<GPUBuffer> culled_instance_vertex_buffer;
  std::unique_ptr<GPUBuffer> culled_instance_count_buffer;
  std::unique_ptr<GPUBuffer> visibility_buffer;
  std::unique_ptr<GPUBuffer> workgroup_sum_buffer;
  std::unique_ptr<GPUBuffer> workgroup_sum_prefix_buffer;
  std::unique_ptr<GPUBuffer> prefix_sum_buffer;
  Assets::Pointer<Material> cull_visibility_material;
  Assets::Pointer<Material> cull_scatter_material;
  Assets::Pointer<Material> cull_prefix_sum_material_first;
  Assets::Pointer<Material> cull_prefix_sum_material_second;
  Assets::Pointer<Material> cull_prefix_sum_material_distribute;

  std::vector<LineInstanceData> line_instances{};
  std::unique_ptr<GPUBuffer> line_instance_buffer{};
  std::uint32_t line_instance_count_this_frame{};
  auto upload_line_instance_data() -> void;

  std::unique_ptr<GPUBuffer> identifier_buffer;

  std::unique_ptr<BloomPass> bloom_pass;
  auto run_bloom_pass(uint32_t uint32) -> void;

  DrawCommandMap draw_commands{};
  IdentifierMap identifiers{};
  DrawCommandMap shadow_draw_commands{};

  std::unique_ptr<GPUBuffer> camera_uniform_buffer;
  std::unique_ptr<GPUBuffer> shadow_camera_buffer;
  std::unique_ptr<GPUBuffer> frustum_buffer;
  auto update_uniform_buffers(std::uint32_t,
                              const glm::mat4& view,
                              const glm::mat4& proj,
                              const glm::mat4& inverse_proj,
                              const glm::vec3&) const -> void;
  auto update_shadow_buffers(std::uint32_t) -> void;
  void update_identifiers();

  auto run_culling_compute_pass(std::uint32_t) -> void;
  auto run_skybox_pass(std::uint32_t) -> void;
  auto run_shadow_pass(std::uint32_t, const DrawList&) -> void;
  auto run_z_prepass(std::uint32_t, const DrawList&) -> void;
  auto run_geometry_pass(std::uint32_t, const DrawList&) -> void;
  auto run_composite_pass(std::uint32_t) -> void;
  auto run_colour_correction_pass(std::uint32_t) -> void;
  auto run_postprocess_passes(const std::uint32_t frame_index) -> void
  {
    run_colour_correction_pass(frame_index);
  }
  auto run_identifier_pass(std::uint32_t, const DrawList&) -> void;

  auto destroy() -> void;

  static inline Assets::Pointer<Image> white_texture{ nullptr };
  static inline Assets::Pointer<Image> black_texture{ nullptr };
};
