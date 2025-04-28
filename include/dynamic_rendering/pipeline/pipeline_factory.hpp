#pragma once

#include <span>
#include <vulkan/vulkan.h>

#include "core/forward.hpp"

#include "pipeline/blueprint_configuration.hpp"
#include "pipeline/ipipeline_factory.hpp"

class PipelineFactory : public IPipelineFactory
{
public:
  ~PipelineFactory() override = default;
  explicit PipelineFactory(const Device& device);
  auto create_pipeline(const PipelineBlueprint&,
                       const PipelineLayoutInfo&) const
    -> std::unique_ptr<CompiledPipeline> override;

private:
  const Device* device;

  auto create_pipeline_layout(const PipelineBlueprint&,
                              std::span<const VkDescriptorSetLayout>,
                              std::span<const VkPushConstantRange>) const
    -> VkPipelineLayout;
};