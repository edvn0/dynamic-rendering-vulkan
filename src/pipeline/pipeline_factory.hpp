#pragma once

#include <span>
#include <vulkan/vulkan.h>

#include "blueprint_configuration.hpp"

class Device;

struct CompiledPipeline
{
  VkPipeline pipeline{ VK_NULL_HANDLE };
  VkPipelineLayout layout{ VK_NULL_HANDLE };
  VkPipelineBindPoint bind_point{ VK_PIPELINE_BIND_POINT_GRAPHICS };
  const Device* device{ nullptr };

  ~CompiledPipeline();
};

class PipelineFactory
{
public:
  explicit PipelineFactory(const Device& device);
  auto create_pipeline(const PipelineBlueprint&,
                       const PipelineLayoutInfo&) const
    -> std::unique_ptr<CompiledPipeline>;

private:
  const Device* device;

  auto create_pipeline_layout(const PipelineBlueprint&,
                              std::span<const VkDescriptorSetLayout>) const
    -> VkPipelineLayout;
};