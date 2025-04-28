#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "core/command_buffer.hpp"
#include "core/config.hpp"
#include "core/device.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image.hpp"
#include "pipeline/blueprint_registry.hpp"
#include "pipeline/compute_pipeline_factory.hpp"
#include "pipeline/pipeline_factory.hpp"
#include "renderer/material.hpp"
#include "window/window.hpp"

struct InstanceData
{
  glm::mat4 transform;
};

struct LightEnvironment
{
  glm::vec3 light_position{ 40.f, 40.f, 40.f };
  glm::vec3 light_color{ 1.f, 1.f, 1.f };
};

struct LineDrawCommand
{
  GPUBuffer* vertex_buffer;
  std::uint32_t vertex_count;
  Material* override_material{ nullptr };
};

struct DrawCommand
{
  GPUBuffer* vertex_buffer;
  IndexBuffer* index_buffer;
  Material* override_material{ nullptr };
  bool casts_shadows{ true };

  bool operator==(const DrawCommand& rhs) const = default;
};

struct DrawCommandHasher
{
  auto operator()(const DrawCommand& dc) const -> std::size_t
  {
    std::size_t h1 = std::hash<GPUBuffer*>{}(dc.vertex_buffer);
    std::size_t h2 = std::hash<IndexBuffer*>{}(dc.index_buffer);
    std::size_t h3 = std::hash<Material*>{}(dc.override_material);
    std::size_t h4 = std::hash<bool>{}(dc.casts_shadows);
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
  }
};

class Renderer
{
public:
  Renderer(const Device&, const BlueprintRegistry&, const Window&);
  ~Renderer();
  auto destroy() -> void;

  auto submit(const DrawCommand&, const glm::mat4& = glm::mat4{ 1.0F }) -> void;
  auto submit_lines(const LineDrawCommand&, const glm::mat4&) -> void;
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

private:
  const Device* device{ nullptr };
  const BlueprintRegistry* blueprint_registry{ nullptr };
  LightEnvironment light_environment;

  std::unique_ptr<CommandBuffer> command_buffer;
  std::unique_ptr<GPUBuffer> instance_vertex_buffer;

  frame_array<VkSemaphore> compute_finished_semaphore{};
  std::unique_ptr<CommandBuffer> compute_command_buffer;

  std::unique_ptr<Image> geometry_image;
  std::unique_ptr<Image> geometry_msaa_image;
  std::unique_ptr<Image> geometry_depth_image;
  std::unique_ptr<Material> z_prepass_material;
  std::unique_ptr<Material> geometry_material;

  std::unique_ptr<Image> shadow_depth_image;
  std::unique_ptr<Material> shadow_material;

  std::unique_ptr<GPUBuffer> gizmo_vertex_buffer;
  std::unique_ptr<Material> gizmo_material;

  std::unique_ptr<Material> line_material;

  std::uint32_t instance_count_this_frame{ 0 };
  std::unique_ptr<Material> cull_instances_compute_material;
  std::unique_ptr<GPUBuffer> culled_instance_vertex_buffer;
  std::unique_ptr<GPUBuffer> culled_instance_count_buffer;

  std::vector<std::pair<LineDrawCommand, glm::mat4>> line_draw_commands{};
  std::unordered_map<DrawCommand, std::vector<InstanceData>, DrawCommandHasher>
    draw_commands{};

  auto update_uniform_buffers(std::uint32_t, const glm::mat4&, const glm::mat4&)
    -> void;
  auto update_shadow_buffers(std::uint32_t) -> void;

  std::unique_ptr<GPUBuffer> camera_uniform_buffer;
  std::unique_ptr<GPUBuffer> shadow_camera_buffer;
  std::unique_ptr<GPUBuffer> frustum_buffer;
  frame_array<VkDescriptorSet> renderer_descriptor_sets{};
  VkDescriptorSetLayout renderer_descriptor_set_layout{};
  VkDescriptorPool descriptor_pool{};
  auto create_descriptor_set_layout() -> void;

  struct Frustum
  {
    std::array<glm::vec4, 6> planes{};

    static auto from_matrix(const glm::mat4& vp) -> Frustum
    {
      Frustum f;
      f.update(vp);
      return f;
    }

    auto update(const glm::mat4& vp) -> void
    {
      const glm::mat4 m = glm::transpose(vp);

      planes[0] = m[3] + m[0];
      planes[1] = m[3] - m[0];
      planes[2] = m[3] + m[1];
      planes[3] = m[3] - m[1];
      planes[4] = m[3] + m[2];
      planes[5] = m[3] - m[2];

      for (auto& plane : planes)
        plane /= glm::length(glm::vec3(plane));
    }

    auto intersects(const glm::vec3& center, float radius) const -> bool
    {
      return std::ranges::none_of(
        planes, [&c = center, &r = radius](const auto& p) {
          return glm::dot(glm::vec3(p), c) + p.w + r < 0.0f;
        });
    }
  };

  Frustum current_frustum;

  bool destroyed{ false };

  using DrawList =
    std::vector<std::tuple<DrawCommand, std::uint32_t, std::uint32_t>>;

  auto run_culling_compute_pass(std::uint32_t) -> void;
  auto run_shadow_pass(std::uint32_t, const DrawList&) -> void;
  auto run_z_prepass(std::uint32_t, const DrawList&) -> void;
  auto run_geometry_pass(std::uint32_t, const DrawList&) -> void;
  auto run_line_pass(std::uint32_t) -> void;
  auto run_gizmo_pass(std::uint32_t) -> void;
};
