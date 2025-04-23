#include "compute_pipeline_factory.hpp"
#include "device.hpp"
#include "shader.hpp"

ComputePipelineFactory::ComputePipelineFactory(const Device& dev)
  : device(&dev)
{
}

auto
ComputePipelineFactory::create_pipeline_layout(const PipelineBlueprint&) const
  -> VkPipelineLayout
{
  VkPipelineLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = 0,
    .pSetLayouts = nullptr,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = nullptr,
  };

  VkPipelineLayout layout{};
  vkCreatePipelineLayout(device->get_device(), &layout_info, nullptr, &layout);
  return layout;
}

auto
ComputePipelineFactory::create_pipeline(
  const PipelineBlueprint& blueprint) const -> CompiledComputePipeline
{
  auto shader = Shader::create(device->get_device(), blueprint.shader_stages);
  const auto& stage_info = blueprint.shader_stages.front();

  VkPipelineShaderStageCreateInfo stage{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    .module = shader->get_module(stage_info.stage),
    .pName = "main",
    .pSpecializationInfo = nullptr,
  };

  VkPipelineLayout layout = create_pipeline_layout(blueprint);

  VkComputePipelineCreateInfo pipeline_info{
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = stage,
    .layout = layout,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  VkPipeline pipeline{};
  vkCreateComputePipelines(device->get_device(),
                           VK_NULL_HANDLE,
                           1,
                           &pipeline_info,
                           nullptr,
                           &pipeline);

  CompiledComputePipeline compiled{ pipeline, layout };
  return compiled;
}
