#pragma once

#include <vulkan/vulkan.h>

#include "blueprint_configuration.hpp"

class Device;
class Shader;

struct CompiledComputePipeline
{
  VkPipeline pipeline{ VK_NULL_HANDLE };
  VkPipelineLayout layout{ VK_NULL_HANDLE };
  VkPipelineBindPoint bind_point{ VK_PIPELINE_BIND_POINT_COMPUTE };
};

class ComputePipelineFactory
{
public:
  explicit ComputePipelineFactory(const Device& device);
  auto create_pipeline(const PipelineBlueprint&) const
    -> CompiledComputePipeline;

private:
  const Device* device;
  auto create_pipeline_layout(const PipelineBlueprint&) const
    -> VkPipelineLayout;
};
