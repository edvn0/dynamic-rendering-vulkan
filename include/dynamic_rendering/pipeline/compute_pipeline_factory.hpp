#pragma once

#include "core/forward.hpp"

#include <span>
#include <vulkan/vulkan.h>

#include "pipeline/blueprint_configuration.hpp"
#include "pipeline/compiled_pipeline.hpp"
#include "pipeline/ipipeline_factory.hpp"

class ComputePipelineFactory : public IPipelineFactory
{
public:
  ~ComputePipelineFactory() override = default;
  explicit ComputePipelineFactory(const Device& device);
  auto create_pipeline(const PipelineBlueprint&,
                       const PipelineLayoutInfo&) const
    -> std::expected<std::unique_ptr<CompiledPipeline>, PipelineError> override;

private:
  const Device* device;
  auto create_pipeline_layout(const PipelineBlueprint&,
                              std::span<const VkDescriptorSetLayout>) const
    -> std::expected<VkPipelineLayout, PipelineError>;
};
