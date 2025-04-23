#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "command_buffer.hpp"
#include "device.hpp"
#include "gpu_buffer.hpp"
#include "image.hpp"
#include "pipeline/blueprint_registry.hpp"
#include "pipeline/compute_pipeline_factory.hpp"
#include "pipeline/pipeline_factory.hpp"
#include "window.hpp"

class Material
{};

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
  auto submit(const DrawCommand&) -> void;
  auto end_frame(std::uint32_t) -> void;
  auto resize(std::uint32_t, std::uint32_t) -> void;
  auto get_output_image() const -> const Image&;
  auto get_command_buffer() const -> CommandBuffer& { return *command_buffer; }
  auto get_compute_command_buffer() const -> CommandBuffer&
  {
    return *compute_command_buffer;
  }

private:
  const Device* device{ nullptr };
  const BlueprintRegistry* blueprint_registry{ nullptr };
  const PipelineFactory* pipeline_factory{ nullptr };
  const ComputePipelineFactory* compute_pipeline_factory{ nullptr };
  const Window* window{ nullptr };

  std::unique_ptr<CommandBuffer> command_buffer;

  frame_array<VkSemaphore> compute_finished_semaphore{};
  std::unique_ptr<CommandBuffer> compute_command_buffer;

  std::unique_ptr<Material> default_geometry_material;

  std::unique_ptr<Image> geometry_image;
  CompiledPipeline geometry_pipeline;
  CompiledComputePipeline test_compute_pipeline;

  std::unordered_map<DrawCommand, std::uint32_t, DrawCommandHasher>
    draw_commands;

  auto run_compute_pass(std::uint32_t) -> void;
};
