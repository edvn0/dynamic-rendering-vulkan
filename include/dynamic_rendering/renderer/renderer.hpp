#pragma once

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
#include "pipeline/blueprint_registry.hpp"
#include "pipeline/compute_pipeline_factory.hpp"
#include "pipeline/pipeline_factory.hpp"
#include "renderer/draw_command.hpp"
#include "renderer/frustum.hpp"
#include "renderer/material.hpp"
#include "window/window.hpp"

#include <BS_thread_pool.hpp>

enum class RenderPass : std::uint8_t
{
  MainGeometry,
  Shadow,
  Gizmo,
  Line,
  ZPrepass,
  ColourCorrection,
  ComputeCulling,
};

inline auto
to_renderpass(std::string_view name) -> RenderPass
{
  if (name == "main_geometry") {
    return RenderPass::MainGeometry;
  } else if (name == "shadow") {
    return RenderPass::Shadow;
  } else if (name == "gizmo") {
    return RenderPass::Gizmo;
  } else if (name == "line") {
    return RenderPass::Line;
  } else if (name == "z_prepass") {
    return RenderPass::ZPrepass;
  } else if (name == "colour_correction") {
    return RenderPass::ColourCorrection;
  } else if (name == "compute_culling") {
    return RenderPass::ComputeCulling;
  }

  assert(false && "Unknown render pass name");
  return RenderPass::MainGeometry;
}

struct LightEnvironment
{
  glm::vec3 light_position{ 40.f, -40.f, 40.f };
  glm::vec4 light_color{ 1.f, 1.f, 1.f, 1.f };
  glm::vec4 ambient_color{ 0.1F, 0.1F, 0.1F, 1.0F };
};

struct LineDrawCommand
{
  GPUBuffer* vertex_buffer;
  std::uint32_t vertex_count;
  Material* override_material{ nullptr };
};
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
  Renderer(const Device&,
           const BlueprintRegistry&,
           const Window&,
           BS::priority_thread_pool&);
  ~Renderer();

  auto submit(const DrawCommand&, const glm::mat4& = glm::mat4{ 1.0F }) -> void;
  auto submit_lines(const glm::vec3&, const glm::vec3&, float, const glm::vec4&)
    -> void;
  auto begin_frame(std::uint32_t, const glm::mat4&, const glm::mat4&) -> void;
  auto end_frame(std::uint32_t) -> void;
  auto resize(std::uint32_t, std::uint32_t) -> void;
  auto get_output_image() const -> const Image&;
  auto get_shadow_image() const -> const Image& { return *shadow_depth_image; }
  auto get_command_buffer() const -> CommandBuffer& { return *command_buffer; }
  auto get_compute_command_buffer() const -> CommandBuffer&
  {
    return *compute_command_buffer;
  }
  auto update_frustum(const glm::mat4& vp) -> void
  {
    current_frustum.update(vp);
  }
  auto get_light_environment() -> auto& { return light_environment; }
  auto get_material_by_name(const std::string& name) -> Material*
  {
    auto current_pass = ::to_renderpass(name);
    switch (current_pass) {
      using enum RenderPass;
      case MainGeometry:
        return geometry_material.get();
      case Shadow:
        return shadow_material.get();
      case Gizmo:
        return gizmo_material.get();
      case Line:
        return line_material.get();
      case ZPrepass:
        return z_prepass_material.get();
      case ColourCorrection:
        return colour_corrected_material.get();
      case ComputeCulling:
        return cull_instances_compute_material.get();
      default:
        assert(false && "Unknown render pass name");
        return nullptr;
    }
  }
  auto get_renderer_descriptor_set_layout(Badge<AssetReloader>) const
    -> VkDescriptorSetLayout;

private:
  const Device* device{ nullptr };
  const BlueprintRegistry* blueprint_registry{ nullptr };
  BS::priority_thread_pool* thread_pool{ nullptr };

  LightEnvironment light_environment;

  std::unique_ptr<CommandBuffer> command_buffer;
  std::unique_ptr<GPUBuffer> instance_vertex_buffer;
  std::unique_ptr<GPUBuffer> instance_shadow_vertex_buffer;

  frame_array<VkSemaphore> compute_finished_semaphore{};
  std::unique_ptr<CommandBuffer> compute_command_buffer;

  std::unique_ptr<Image> geometry_image;
  std::unique_ptr<Image> geometry_msaa_image;
  std::unique_ptr<Image> geometry_depth_image;
  std::unique_ptr<Material> z_prepass_material;
  std::unique_ptr<Material> geometry_material;

  std::unique_ptr<Image> colour_corrected_image;
  std::unique_ptr<Material> colour_corrected_material;

  std::unique_ptr<Image> shadow_depth_image;
  std::unique_ptr<Material> shadow_material;

  std::unique_ptr<GPUBuffer> gizmo_vertex_buffer;
  std::unique_ptr<Material> gizmo_material;

  std::unique_ptr<Material> line_material;

  std::uint32_t instance_count_this_frame{ 0 };
  std::unique_ptr<Material> cull_instances_compute_material;
  std::unique_ptr<GPUBuffer> culled_instance_vertex_buffer;
  std::unique_ptr<GPUBuffer> culled_instance_count_buffer;

  std::vector<LineInstanceData> line_instances{};
  std::unique_ptr<GPUBuffer> line_instance_buffer{};
  std::uint32_t line_instance_count_this_frame{};
  auto upload_line_instance_data() -> void;

  DrawCommandMap draw_commands{};
  DrawCommandMap shadow_draw_commands{};

  auto update_uniform_buffers(std::uint32_t, const glm::mat4&, const glm::mat4&)
    -> void;
  auto update_shadow_buffers(std::uint32_t) -> void;

  std::unique_ptr<GPUBuffer> camera_uniform_buffer;
  std::unique_ptr<GPUBuffer> shadow_camera_buffer;
  std::unique_ptr<GPUBuffer> frustum_buffer;
  frame_array<VkDescriptorSet> renderer_descriptor_sets{};
  VkDescriptorSetLayout renderer_descriptor_set_layout{};
  VkDescriptorPool descriptor_pool{};

  auto run_culling_compute_pass(std::uint32_t) -> void;
  auto run_shadow_pass(std::uint32_t, const DrawList&) -> void;
  auto run_z_prepass(std::uint32_t, const DrawList&) -> void;
  auto run_geometry_pass(std::uint32_t, const DrawList&) -> void;
  auto run_line_pass(std::uint32_t) -> void;
  auto run_gizmo_pass(std::uint32_t) -> void;
  auto run_colour_correction_pass(std::uint32_t frame_index) -> void;
  auto run_postprocess_passes(std::uint32_t frame_index) -> void
  {
    run_colour_correction_pass(frame_index);
  }

  Frustum current_frustum;
  Frustum light_frustum;

  auto destroy() -> void;
  auto create_descriptor_set_layout_from_metadata() -> void;
  auto finalize_renderer_descriptor_sets() -> void;
};
