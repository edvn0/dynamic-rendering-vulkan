#pragma once

#include "shader.hpp"

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

struct ShaderPaths
{
  std::string vertex;
  std::string fragment;
};

struct VertexBinding
{
  std::uint32_t binding;
  std::uint32_t stride;
  VkVertexInputRate input_rate;
};

struct VertexAttribute
{
  std::uint32_t location;
  std::uint32_t binding;
  VkFormat format;
  std::uint32_t offset;
};

struct PipelineBlueprint
{
  std::string name;
  std::vector<ShaderStageInfo> shader_stages;
  std::vector<VertexBinding> bindings{};
  std::vector<VertexAttribute> attributes{};
  VkCullModeFlags cull_mode{ VK_CULL_MODE_BACK_BIT };
  VkPolygonMode polygon_mode{ VK_POLYGON_MODE_FILL };
  bool blend_enable{ false };
  bool depth_test{ false };
  bool depth_write{ false };

  auto hash() const -> std::size_t;
};
