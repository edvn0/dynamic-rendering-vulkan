#include "pipeline/compute_pipeline_factory.hpp"

#include "core/device.hpp"
#include "pipeline/shader.hpp"

ComputePipelineFactory::ComputePipelineFactory(const Device& dev)
  : device(&dev)
{
}

auto
ComputePipelineFactory::create_pipeline_layout(
  const PipelineBlueprint&,
  std::span<const VkDescriptorSetLayout> layouts,
  std::span<const VkPushConstantRange> push_constants) const
  -> std::expected<VkPipelineLayout, PipelineError>
{
  VkPipelineLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
    .pSetLayouts = layouts.data(),
    .pushConstantRangeCount = static_cast<std::uint32_t>(push_constants.size()),
    .pPushConstantRanges = push_constants.data(),
  };

  VkPipelineLayout layout{};
  if (vkCreatePipelineLayout(
        device->get_device(), &layout_info, nullptr, &layout) != VK_SUCCESS) {
    return std::unexpected(PipelineError{
      .message = "Failed to create pipeline layout",
      .code = PipelineError::Code::pipeline_layout_creation_failed,
    });
  }

  return layout;
}

auto
ComputePipelineFactory::create_pipeline(const PipelineBlueprint& blueprint,
                                        const PipelineLayoutInfo& layout_info)
  const -> std::expected<std::unique_ptr<CompiledPipeline>, PipelineError>
{
  auto shader_result = Shader::create(*device, blueprint.shader_stages);
  if (!shader_result) {
    return std::unexpected(PipelineError{
      .message = "Shader creation failed: " + shader_result.error().message,
      .code = PipelineError::Code::pipeline_creation_failed,
    });
  }

  auto shader = std::move(shader_result.value());
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

  std::vector<VkDescriptorSetLayout> layouts;
  if (layout_info.renderer_set_layout != VK_NULL_HANDLE)
    layouts.push_back(layout_info.renderer_set_layout);
  layouts.insert(layouts.end(),
                 layout_info.material_sets.begin(),
                 layout_info.material_sets.end());

  auto layout_result =
    create_pipeline_layout(blueprint, layouts, layout_info.push_constants);
  if (!layout_result)
    return std::unexpected(layout_result.error());

  VkComputePipelineCreateInfo pipeline_info{
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = stage,
    .layout = layout_result.value(),
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  VkPipeline pipeline{};
  if (vkCreateComputePipelines(device->get_device(),
                               VK_NULL_HANDLE,
                               1,
                               &pipeline_info,
                               nullptr,
                               &pipeline) != VK_SUCCESS) {
    return std::unexpected(PipelineError{
      .message = "Failed to create compute pipeline",
      .code = PipelineError::Code::pipeline_creation_failed,
    });
  }

  return std::make_unique<CompiledPipeline>(pipeline,
                                            layout_result.value(),
                                            VK_PIPELINE_BIND_POINT_COMPUTE,
                                            std::move(shader),
                                            device);
}
