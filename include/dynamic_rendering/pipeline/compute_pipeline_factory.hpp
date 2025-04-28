#pragma once

#include <span>
#include <vulkan/vulkan.h>

#include "blueprint_configuration.hpp"
#include "compiled_pipeline.hpp"
#include "ipipeline_factory.hpp"

class Device;
class Shader;

class ComputePipelineFactory : public IPipelineFactory
{
public:
  ~ComputePipelineFactory() override = default;
  explicit ComputePipelineFactory(const Device& device);
  auto create_pipeline(const PipelineBlueprint&,
                       const PipelineLayoutInfo&) const
    -> std::unique_ptr<CompiledPipeline> override;

private:
  const Device* device;
  auto create_pipeline_layout(const PipelineBlueprint&,
                              std::span<const VkDescriptorSetLayout>) const
    -> VkPipelineLayout;
};
