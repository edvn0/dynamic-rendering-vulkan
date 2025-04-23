#pragma once

#include "shader.hpp"

#include <optional>
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

struct Attachment
{
  VkFormat format{ VK_FORMAT_UNDEFINED };
  bool blend_enable{ false };
  bool write_mask_rgba{ true };

  auto is_depth() const -> bool
  {
    return format == VK_FORMAT_D32_SFLOAT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT ||
           format == VK_FORMAT_D16_UNORM_S8_UINT ||
           format == VK_FORMAT_D16_UNORM ||
           format == VK_FORMAT_D32_SFLOAT_S8_UINT;
  }
  auto is_color() const -> bool { return !is_depth(); }
};

struct PipelineBlueprint
{
  std::string name;
  std::vector<ShaderStageInfo> shader_stages;
  std::vector<VertexBinding> bindings{};
  std::vector<VertexAttribute> attributes{};
  std::vector<Attachment> attachments{};
  std::optional<Attachment> depth_attachment{};
  VkCullModeFlags cull_mode{ VK_CULL_MODE_BACK_BIT };
  VkPolygonMode polygon_mode{ VK_POLYGON_MODE_FILL };
  bool blend_enable{ false };
  bool depth_test{ false };
  bool depth_write{ false };

  auto hash() const -> std::size_t;
};
