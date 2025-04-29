#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/config.hpp"
#include "core/forward.hpp"
#include "pipeline/compute_pipeline_factory.hpp"
#include "pipeline/pipeline_factory.hpp"

#include <vulkan/vulkan.h>

class Material
{
public:
  static auto create(const Device& device,
                     const PipelineBlueprint& blueprint,
                     VkDescriptorSetLayout renderer_set_layout)
    -> std::unique_ptr<Material>;

  ~Material();

  auto upload(std::string_view name, const GPUBuffer* buffer) -> void;
  auto upload(std::string_view name, const Image* image) -> void;
  auto prepare_for_rendering(std::uint32_t frame_index)
    -> const VkDescriptorSet&;

  auto get_pipeline() const -> const CompiledPipeline& { return *pipeline; }
  auto get_descriptor_set(std::uint32_t frame_index) const -> const auto&
  {
    return descriptor_sets[frame_index];
  }

  auto reload(const PipelineBlueprint&, VkDescriptorSetLayout) -> void;

private:
  std::unordered_map<std::string, std::unique_ptr<GPUBinding>> bindings;
  std::unordered_map<std::string, std::tuple<std::uint32_t, std::uint32_t>>
    binding_info;
  frame_array<std::unordered_set<std::string>> per_frame_dirty_flags;

  const Device* device{ nullptr };
  frame_array<VkDescriptorSet> descriptor_sets{};
  VkDescriptorSetLayout descriptor_set_layout{};
  VkDescriptorPool descriptor_pool{};
  bool destroyed{ false };

  // Materials own the pipeline.
  std::unique_ptr<CompiledPipeline> pipeline{ nullptr };
  std::size_t pipeline_hash{ 0 };

  Material(const Device& device,
           frame_array<VkDescriptorSet>&&,
           std::span<const VkDescriptorSetLayout>,
           VkDescriptorPool,
           std::unique_ptr<CompiledPipeline>,
           std::unordered_map<std::string,
                              std::tuple<std::uint32_t, std::uint32_t>>&&);

  auto destroy() -> void;

  static inline std::unique_ptr<IPipelineFactory> pipeline_factory{ nullptr };
  static inline std::unique_ptr<IPipelineFactory> compute_pipeline_factory{
    nullptr
  };
  auto rebuild_pipeline(const PipelineBlueprint&, VkDescriptorSetLayout)
    -> std::unique_ptr<CompiledPipeline>;
};
