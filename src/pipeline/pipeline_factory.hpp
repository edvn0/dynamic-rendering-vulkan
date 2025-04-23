#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "blueprint_configuration.hpp"

class Device;

struct CompiledPipeline
{
  VkPipeline pipeline{ VK_NULL_HANDLE };
  VkPipelineLayout layout{ VK_NULL_HANDLE };
};

class PipelineFactory
{
public:
  explicit PipelineFactory(const Device& device);
  auto create_pipeline(const PipelineBlueprint& blueprint) -> CompiledPipeline;

private:
  const Device* device;

  auto create_pipeline_layout(const PipelineBlueprint& blueprint)
    -> VkPipelineLayout;
  auto create_shader_module(const std::string& path) -> VkShaderModule;

  std::unordered_map<std::size_t, CompiledPipeline> pipelines;
};