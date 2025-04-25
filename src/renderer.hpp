#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "command_buffer.hpp"
#include "config.hpp"
#include "device.hpp"
#include "gpu_buffer.hpp"
#include "image.hpp"
#include "material.hpp"
#include "pipeline/blueprint_registry.hpp"
#include "pipeline/compute_pipeline_factory.hpp"
#include "pipeline/pipeline_factory.hpp"
#include "window.hpp"

struct InstanceData
{
  glm::vec4 rotation{};              // Quaternion
  glm::vec4 translation_and_scale{}; // Translation and scale
  glm::vec4 non_uniform_scale{};     // Non-uniform scale
};

struct DrawCommand
{
  GPUBuffer* vertex_buffer;
  IndexBuffer* index_buffer;
  Material* override_material{ nullptr };

  bool operator==(const DrawCommand& rhs) const = default;
};

struct DrawCommandHasher
{
  auto operator()(const DrawCommand& dc) const -> std::size_t
  {
    std::size_t h1 = std::hash<GPUBuffer*>{}(dc.vertex_buffer);
    std::size_t h2 = std::hash<IndexBuffer*>{}(dc.index_buffer);
    std::size_t h3 = std::hash<Material*>{}(dc.override_material);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

class Renderer
{
public:
  Renderer(const Device&,
           const BlueprintRegistry&,
           const PipelineFactory&,
           const ComputePipelineFactory&,
           const Window&);
  ~Renderer();
  auto destroy() -> void;

  auto submit(const DrawCommand&, const glm::mat4& = glm::mat4{ 1.0F }) -> void;
  auto end_frame(std::uint32_t,
                 const glm::mat4& projection,
                 const glm::mat4& view) -> void;
  auto resize(std::uint32_t, std::uint32_t) -> void;
  auto get_output_image() const -> const Image&;
  auto get_command_buffer() const -> CommandBuffer& { return *command_buffer; }
  auto get_compute_command_buffer() const -> CommandBuffer&
  {
    return *compute_command_buffer;
  }
  auto update_frustum(const glm::mat4& vp) -> void
  {
    current_frustum = Frustum::from_matrix(vp);
  }

private:
  const Device* device{ nullptr };
  const BlueprintRegistry* blueprint_registry{ nullptr };
  const PipelineFactory* pipeline_factory{ nullptr };
  const ComputePipelineFactory* compute_pipeline_factory{ nullptr };
  const Window* window{ nullptr };

  std::unique_ptr<CommandBuffer> command_buffer;
  std::unique_ptr<GPUBuffer> instance_vertex_buffer;

  frame_array<VkSemaphore> compute_finished_semaphore{};
  std::unique_ptr<CommandBuffer> compute_command_buffer;

  std::unique_ptr<Material> default_geometry_material;

  std::unique_ptr<Image> geometry_image;
  std::unique_ptr<Image> geometry_depth_image;
  std::unique_ptr<CompiledPipeline> geometry_pipeline;
  std::unique_ptr<CompiledPipeline> z_prepass_pipeline;
  std::unique_ptr<CompiledComputePipeline> test_compute_pipeline;

  std::unordered_map<DrawCommand, std::vector<InstanceData>, DrawCommandHasher>
    draw_commands{};

  auto run_compute_pass(std::uint32_t) -> void;
  auto update_uniform_buffers(std::uint32_t, const glm::mat4&) -> void;

  std::unique_ptr<GPUBuffer> camera_uniform_buffer;
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
      const glm::mat4 m = glm::transpose(vp);

      f.planes[0] = m[3] + m[0];
      f.planes[1] = m[3] - m[0];
      f.planes[2] = m[3] + m[1];
      f.planes[3] = m[3] - m[1];
      f.planes[4] = m[3] + m[2];
      f.planes[5] = m[3] - m[2];

      for (auto& plane : f.planes)
        plane /= glm::length(glm::vec3(plane));

      return f;
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

  using DrawList = std::vector<std::pair<DrawCommand, std::uint32_t>>;
  auto run_geometry_pass(std::uint32_t, const DrawList&) -> void;
  auto run_z_prepass(std::uint32_t, const DrawList&) -> void;
};
