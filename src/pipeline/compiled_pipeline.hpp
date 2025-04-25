#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "forward.hpp"

struct CompiledPipeline
{
  VkPipeline pipeline{ VK_NULL_HANDLE };
  VkPipelineLayout layout{ VK_NULL_HANDLE };
  VkPipelineBindPoint bind_point{ VK_PIPELINE_BIND_POINT_GRAPHICS };
  std::unique_ptr<Shader> shader{ nullptr };
  const Device* device{ nullptr };

  ~CompiledPipeline();
};
