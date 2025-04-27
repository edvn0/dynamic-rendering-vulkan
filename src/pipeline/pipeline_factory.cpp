#include "pipeline/pipeline_factory.hpp"

#include "pipeline/compiled_pipeline.hpp"

#include "device.hpp"

#include <array>
#include <vulkan/vulkan_core.h>

static auto to_vk_stage = [](ShaderStage stage) {
  switch (stage) {
    case ShaderStage::vertex:
      return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::fragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderStage::compute:
      return VK_SHADER_STAGE_COMPUTE_BIT;
    case ShaderStage::raygen:
      return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case ShaderStage::closest_hit:
      return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case ShaderStage::miss:
      return VK_SHADER_STAGE_MISS_BIT_KHR;
    default:
      assert(false && "Unsupported shader stage");
      return VK_SHADER_STAGE_ALL; // fallback
  }
};

PipelineFactory::PipelineFactory(const Device& dev)
  : device(&dev)
{
}

auto
PipelineFactory::create_pipeline_layout(
  const PipelineBlueprint&,
  std::span<const VkDescriptorSetLayout> layouts,
  std::span<const VkPushConstantRange> ranges) const -> VkPipelineLayout
{
  VkPipelineLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
    .pSetLayouts = layouts.data(),
    .pushConstantRangeCount = static_cast<std::uint32_t>(ranges.size()),
    .pPushConstantRanges = ranges.data(),
  };

  VkPipelineLayout layout{};
  vkCreatePipelineLayout(device->get_device(), &layout_info, nullptr, &layout);
  return layout;
}

auto
PipelineFactory::create_pipeline(const PipelineBlueprint& blueprint,
                                 const PipelineLayoutInfo& layout_info) const
  -> std::unique_ptr<CompiledPipeline>
{
  auto shader = Shader::create(*device, blueprint.shader_stages);

  std::vector<VkPipelineShaderStageCreateInfo> shader_infos;
  for (const auto& info : blueprint.shader_stages) {
    shader_infos.push_back({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = to_vk_stage(info.stage),
      .module = shader->get_module(info.stage),
      .pName = "main",
      .pSpecializationInfo = nullptr,
    });
  }

  std::vector<VkVertexInputBindingDescription> bindings;
  for (const auto& b : blueprint.bindings) {
    bindings.push_back({
      .binding = b.binding,
      .stride = b.stride,
      .inputRate = b.input_rate,
    });
  }

  std::vector<VkVertexInputAttributeDescription> attributes;
  for (const auto& a : blueprint.attributes) {
    attributes.push_back({
      .location = a.location,
      .binding = a.binding,
      .format = a.format,
      .offset = a.offset,
    });
  }

  VkPipelineVertexInputStateCreateInfo vertex_input{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount =
      static_cast<std::uint32_t>(bindings.size()),
    .pVertexBindingDescriptions = bindings.data(),
    .vertexAttributeDescriptionCount =
      static_cast<std::uint32_t>(attributes.size()),
    .pVertexAttributeDescriptions = attributes.data(),
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .topology = blueprint.topology,
    .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = blueprint.polygon_mode,
    .cullMode = blueprint.cull_mode,
    .frontFace = blueprint.winding,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment{
    .blendEnable = blueprint.blend_enable,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendStateCreateInfo color_blend{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = 1,
    .pAttachments = &color_blend_attachment,
    .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
  };

  VkPipelineDepthStencilStateCreateInfo depth_stencil{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthTestEnable = blueprint.depth_test,
    .depthWriteEnable = blueprint.depth_write,
    .depthCompareOp = blueprint.depth_compare_op,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = VK_FALSE,
    .front = { 
        .failOp = VK_STENCIL_OP_KEEP,
        .passOp = VK_STENCIL_OP_KEEP,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0,
        .writeMask = 0,
        .reference = 0,
    },
    .back = { 
        .failOp = VK_STENCIL_OP_KEEP,
        .passOp = VK_STENCIL_OP_KEEP,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0,
        .writeMask = 0,
        .reference = 0,
    },
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f,
  };

  // Dynamic state viewport, scissor
  const std::array<VkDynamicState, 2> dynamic_states{
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic_state{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
    .pDynamicStates = dynamic_states.data(),
  };

  VkPipelineViewportStateCreateInfo viewport_state{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 1,
    .scissorCount = 1,
  };

  auto sample_count = device->get_max_sample_count(blueprint.msaa_samples);

  VkPipelineMultisampleStateCreateInfo multisample_state{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .rasterizationSamples = static_cast<VkSampleCountFlagBits>(sample_count),
    .sampleShadingEnable = VK_FALSE,
    .minSampleShading = 0.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE,
  };

  std::vector<VkFormat> color_attachment_formats;
  for (const auto& attachment : blueprint.attachments)
    if (attachment.is_color())
      color_attachment_formats.push_back(attachment.format);

  VkFormat depth_format = VK_FORMAT_UNDEFINED;
  if (blueprint.depth_attachment.has_value() &&
      blueprint.depth_attachment->is_depth())
    depth_format = blueprint.depth_attachment->format;

  VkPipelineRenderingCreateInfo rendering_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .pNext = nullptr,
    .viewMask = 0,
    .colorAttachmentCount =
      static_cast<std::uint32_t>(color_attachment_formats.size()),
    .pColorAttachmentFormats = color_attachment_formats.data(),
    .depthAttachmentFormat = depth_format,
    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  std::vector<VkDescriptorSetLayout> layouts{
    layout_info.renderer_set_layout,
  };
  layouts.insert(layouts.end(),
                 layout_info.material_sets.begin(),
                 layout_info.material_sets.end());
  VkPipelineLayout layout =
    create_pipeline_layout(blueprint, layouts, layout_info.push_constants);

  VkGraphicsPipelineCreateInfo pipeline_info{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &rendering_info,
    .flags = 0,
    .stageCount = static_cast<std::uint32_t>(shader_infos.size()),
    .pStages = shader_infos.data(),
    .pVertexInputState = &vertex_input,
    .pInputAssemblyState = &input_assembly,
    .pTessellationState = nullptr,
    .pViewportState = &viewport_state,
    .pRasterizationState = &rasterizer,
    .pMultisampleState = &multisample_state,
    .pDepthStencilState = &depth_stencil,
    .pColorBlendState = &color_blend,
    .pDynamicState = &dynamic_state,
    .layout = layout,
    .renderPass = VK_NULL_HANDLE,
    .subpass = 0,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  VkPipeline pipeline;
  if (vkCreateGraphicsPipelines(device->get_device(),
                                VK_NULL_HANDLE,
                                1,
                                &pipeline_info,
                                nullptr,
                                &pipeline) != VK_SUCCESS) {
    std::cerr << "What" << std::endl;
    assert(false && "Could not create pipeline");
  }

  return std::make_unique<CompiledPipeline>(pipeline,
                                            layout,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            std::move(shader),
                                            device);
}