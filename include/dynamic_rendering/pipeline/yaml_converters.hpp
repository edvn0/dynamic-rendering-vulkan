#pragma once

#include "blueprint_configuration.hpp"

#include <cassert>
#include <expected>
#include <string>
#include <yaml-cpp/yaml.h>

#include <iostream>

namespace YAML {

inline void
set_default_vertex_input(PipelineBlueprint& rhs)
{
  rhs.bindings = {
    { .binding = 0, .stride = 48, .input_rate = VK_VERTEX_INPUT_RATE_VERTEX },
    { .binding = 1,
      .stride = sizeof(float) * 4 * 4,
      .input_rate = VK_VERTEX_INPUT_RATE_INSTANCE },
  };

  rhs.attributes = {
    { .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = 0 }, // position
    { .location = 1,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = 12 }, // normal
    { .location = 2,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = 24 }, // texcoord
    { .location = 3,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = 32 }, // tangent (xyz + handedness)
    { .location = 4,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = 0 }, // model matrix row 0
    { .location = 5,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = 16 }, // row 1
    { .location = 6,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = 32 }, // row 2
    { .location = 7,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = 48 }, // row 3
  };
}

// Error type for Vulkan conversion failures
struct ConversionError
{
  std::string message;
  explicit ConversionError(std::string msg)
    : message(std::move(msg))
  {
  }
};

// Helper functions for Vulkan type conversions using std::expected
std::expected<VkFormat, ConversionError> inline string_to_vk_format(
  const std::string& format_str)
{
  if (format_str == "b8g8r8a8_srgb")
    return VK_FORMAT_B8G8R8A8_SRGB;
  else if (format_str == "b8g8r8a8_unorm")
    return VK_FORMAT_B8G8R8A8_UNORM;
  else if (format_str == "r8g8b8a8_unorm")
    return VK_FORMAT_R8G8B8A8_UNORM;
  else if (format_str == "r32g32b32a32_sfloat")
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  else if (format_str == "b32g32r32a32_sfloat") {
    std::cout
      << "Vulkan only supports RGBA32, not BGRA32, falling back to RGBA32.\n";
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  } else if (format_str == "d32_sfloat")
    return VK_FORMAT_D32_SFLOAT;
  else if (format_str == "d24_unorm_s8")
    return VK_FORMAT_D24_UNORM_S8_UINT;
  else if (format_str == "r32_uint")
    return VK_FORMAT_R32_UINT;

  return std::unexpected(ConversionError("Unsupported format: " + format_str));
}

std::expected<std::string, ConversionError> inline vk_format_to_string(
  VkFormat format)
{
  switch (format) {
    case VK_FORMAT_B8G8R8A8_SRGB:
      return "b8g8r8a8_srgb";
    case VK_FORMAT_B8G8R8A8_UNORM:
      return "b8g8r8a8_unorm";
    case VK_FORMAT_R8G8B8A8_UNORM:
      return "r8g8b8a8_unorm";
    case VK_FORMAT_D32_SFLOAT:
      return "d32_sfloat";
    case VK_FORMAT_D24_UNORM_S8_UINT:
      return "d24_unorm_s8";
    case VK_FORMAT_R32_UINT:
      return "r32_uint";
    default:
      return std::unexpected(ConversionError("Unsupported VkFormat value"));
  }
}

std::expected<VkPrimitiveTopology, ConversionError> inline string_to_topology(
  const std::string& topo)
{
  if (topo == "triangle-list")
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  if (topo == "triangle-strip")
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  if (topo == "line-list")
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  if (topo == "line-strip")
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  if (topo == "point-list")
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

  return std::unexpected(ConversionError("Unsupported topology: " + topo));
}

std::
  expected<VkFormat, ConversionError> inline string_to_vertex_attribute_format(
    const std::string& fmt)
{
  if (fmt == "vec2")
    return VK_FORMAT_R32G32_SFLOAT;
  if (fmt == "vec3")
    return VK_FORMAT_R32G32B32_SFLOAT;
  if (fmt == "vec4")
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  if (fmt == "r32_sfloat")
    return VK_FORMAT_R32_SFLOAT;
  if (fmt == "r32_uint")
    return VK_FORMAT_R32_UINT;

  return std::unexpected(
    ConversionError("Unsupported vertex attribute format: " + fmt));
}

std::expected<VkCullModeFlags, ConversionError> inline string_to_cull_mode(
  const std::string& cm)
{
  if (cm == "none")
    return VK_CULL_MODE_NONE;
  else if (cm == "front")
    return VK_CULL_MODE_FRONT_BIT;
  else if (cm == "back")
    return VK_CULL_MODE_BACK_BIT;
  else if (cm == "both")
    return VK_CULL_MODE_FRONT_AND_BACK;

  return std::unexpected(ConversionError("Unsupported cull mode: " + cm));
}

std::expected<VkPolygonMode, ConversionError> inline string_to_polygon_mode(
  const std::string& pm)
{
  if (pm == "line")
    return VK_POLYGON_MODE_LINE;
  else if (pm == "point")
    return VK_POLYGON_MODE_POINT;
  else if (pm == "fill")
    return VK_POLYGON_MODE_FILL;

  return std::unexpected(ConversionError("Unsupported polygon mode: " + pm));
}

std::expected<VkFrontFace, ConversionError> inline string_to_winding_mode(
  const std::string& pm)
{
  if (pm == "cw" || pm == "clockwise")
    return VK_FRONT_FACE_CLOCKWISE;
  else if (pm == "ccw" || pm == "counter-clockwise" || pm == "counterclockwise")
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;

  return std::unexpected(ConversionError("Unsupported winding mode: " + pm));
}

std::expected<VkCompareOp, ConversionError> inline string_to_compare_op(
  const std::string& cmp)
{
  if (cmp == "less")
    return VK_COMPARE_OP_LESS;
  else if (cmp == "less_equal" || cmp == "less_or_equal")
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  else if (cmp == "greater")
    return VK_COMPARE_OP_GREATER;
  else if (cmp == "greater_equal" || cmp == "greater_or_equal")
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  else if (cmp == "always")
    return VK_COMPARE_OP_ALWAYS;
  else if (cmp == "never")
    return VK_COMPARE_OP_NEVER;
  else if (cmp == "equal")
    return VK_COMPARE_OP_EQUAL;
  else if (cmp == "not_equal")
    return VK_COMPARE_OP_NOT_EQUAL;

  return std::unexpected(
    ConversionError("Unsupported depth compare_op: " + cmp));
}

std::expected<ShaderStage, ConversionError> inline string_to_shader_stage(
  const std::string& stage_str)
{
  if (stage_str == "vertex")
    return ShaderStage::vertex;
  else if (stage_str == "fragment")
    return ShaderStage::fragment;
  else if (stage_str == "compute")
    return ShaderStage::compute;
  else if (stage_str == "raygen")
    return ShaderStage::raygen;
  else if (stage_str == "closest_hit")
    return ShaderStage::closest_hit;
  else if (stage_str == "miss")
    return ShaderStage::miss;

  return std::unexpected(
    ConversionError("Unsupported shader stage: " + stage_str));
}

// YAML converters that use the expected-based helpers
inline void
log_error(const std::string& msg)
{
  Logger::log_error("[Pipeline YAML Error] {}", msg);
}

inline void
log_warning(const std::string& msg)
{
  Logger::log_warning("[Pipeline YAML Warning] {}", msg);
}

template<>
struct convert<VkFormat>
{
  static Node encode(const VkFormat& rhs)
  {
    auto result = vk_format_to_string(rhs);
    if (result.has_value()) {
      return Node(result.value());
    }
    log_error(result.error().message);
    return Node{};
  }

  static bool decode(const Node& node, VkFormat& rhs)
  {
    const auto format_str = node.as<std::string>();
    auto result = string_to_vk_format(format_str);
    if (result.has_value()) {
      rhs = result.value();
      return true;
    }
    log_error(result.error().message);
    return false;
  }
};

template<>
struct convert<ShaderStageInfo>
{
  static bool decode(const Node& node, ShaderStageInfo& info)
  {
    const auto stage_str = node["stage"].as<std::string>();
    auto stage_result = string_to_shader_stage(stage_str);
    if (!stage_result.has_value()) {
      log_error(stage_result.error().message);
      return false;
    }

    info.stage = stage_result.value();

    if (node["empty"] && node["empty"].as<bool>() == true) {
      info.empty = true;
      info.filepath = "";
    } else {
      info.filepath = node["path"].as<std::string>();
    }

    return true;
  }
};

template<>
struct convert<Attachment>
{
  static bool decode(const Node& node, Attachment& out)
  {
    const auto format_str = node["format"].as<std::string>();
    auto format_result = string_to_vk_format(format_str);
    if (!format_result.has_value()) {
      log_error(format_result.error().message);
      return false;
    }

    out.format = format_result.value();

    if (node["blend_enable"])
      out.blend_enable = node["blend_enable"].as<bool>();

    if (node["write_mask_rgba"])
      out.write_mask_rgba = node["write_mask_rgba"].as<bool>();

    return true;
  }
};

template<>
struct convert<VertexBinding>
{
  static bool decode(const Node& node, VertexBinding& rhs)
  {
    rhs.binding = node["binding"].as<std::uint32_t>();
    rhs.stride = node["stride"].as<std::uint32_t>();
    auto input_rate_str = node["input_rate"].as<std::string>();
    rhs.input_rate = input_rate_str == "instance"
                       ? VK_VERTEX_INPUT_RATE_INSTANCE
                       : VK_VERTEX_INPUT_RATE_VERTEX;
    return true;
  }
};

template<>
struct convert<VertexAttribute>
{
  static bool decode(const Node& node, VertexAttribute& rhs)
  {
    rhs.location = node["location"].as<std::uint32_t>();
    rhs.binding = node["binding"].as<std::uint32_t>();
    rhs.offset = node["offset"].as<std::uint32_t>();

    auto format_result =
      string_to_vertex_attribute_format(node["format"].as<std::string>());
    if (!format_result.has_value()) {
      log_error(format_result.error().message);
      return false;
    }

    rhs.format = format_result.value();
    return true;
  }
};

template<>
struct convert<DepthBias>
{
  static bool decode(const Node& node, DepthBias& rhs)
  {
    rhs.constant_factor = node["constant_factor"].as<float>(1.75F);
    rhs.clamp = node["clamp"].as<float>(0.0F);
    rhs.slope_factor = node["slope_factor"].as<float>(0.5F);
    return true;
  }
};

template<>
struct convert<PipelineBlueprint>
{
  static bool decode(const Node& node, PipelineBlueprint& rhs)
  {
    rhs.shader_stages = node["shaders"].as<std::vector<ShaderStageInfo>>();

    if (node["vertex_input"]) {
      if (node["vertex_input"]["bindings"])
        rhs.bindings =
          node["vertex_input"]["bindings"].as<std::vector<VertexBinding>>();
      if (node["vertex_input"]["attributes"])
        rhs.attributes =
          node["vertex_input"]["attributes"].as<std::vector<VertexAttribute>>();
    } else {
      set_default_vertex_input(rhs);
    }

    if (node["rasterization"]) {
      auto& rast = node["rasterization"];

      auto cull_result =
        string_to_cull_mode(rast["cull_mode"].as<std::string>("back"));

      if (!cull_result.has_value()) {
        log_error("Invalid cull mode: " + rast["cull_mode"].as<std::string>());
        return false;
      }
      rhs.cull_mode = cull_result.value();

      auto polygon_result =
        string_to_polygon_mode(rast["polygon_mode"].as<std::string>("fill"));

      if (!polygon_result.has_value()) {
        log_error(polygon_result.error().message);
        return false;
      }
      rhs.polygon_mode = polygon_result.value();

      auto winding_result = string_to_winding_mode(
        rast["winding"].as<std::string>("counter-clockwise"));

      if (!winding_result.has_value()) {
        log_error(winding_result.error().message);
        return false;
      }
      rhs.winding = winding_result.value();
    }

    auto topology_result =
      string_to_topology(node["topology"].as<std::string>("triangle-list"));

    if (!topology_result.has_value()) {
      log_error(topology_result.error().message);
      return false;
    }
    rhs.topology = topology_result.value();

    if (node["blend"])
      rhs.blend_enable = node["blend"]["enable"].as<bool>(false);

    if (node["depth_stencil"]) {
      auto& depth_stencil = node["depth_stencil"];
      rhs.depth_test = depth_stencil["depth_test"].as<bool>(false);
      rhs.depth_write = depth_stencil["depth_write"].as<bool>(false);

      if (depth_stencil["format"]) {
        const auto format_result = depth_stencil["format"].as<VkFormat>();
        rhs.depth_attachment = Attachment{ .format = format_result };
      }

      if (depth_stencil["compare_op"]) {
        auto compare_result =
          string_to_compare_op(depth_stencil["compare_op"].as<std::string>());

        if (!compare_result.has_value()) {
          log_error(compare_result.error().message);
          return false;
        }
        rhs.depth_compare_op = compare_result.value();
      }

      if (depth_stencil["depth_bias"]) {
        auto bias = depth_stencil["depth_bias"].as<DepthBias>();
        rhs.depth_bias = bias;
      }

      if (!rhs.depth_test && rhs.depth_write) {
        log_error("Invalid pipeline config: depth_write requires depth_test");
        return false;
      }

      if (depth_stencil["compare_op"] && !rhs.depth_test) {
        log_warning(rhs.name +
                    "- Using depth compare_op without depth_test may lead to "
                    "undefined behavior");
      }

      if ((rhs.depth_compare_op == VK_COMPARE_OP_GREATER ||
           rhs.depth_compare_op == VK_COMPARE_OP_GREATER_OR_EQUAL) &&
          !rhs.depth_write) {
        log_warning(
          rhs.name +
          "- Greater depth compare used but depth_write is false. This "
          "may be unintended.");
      }
    }

    if (node["attachments"])
      rhs.attachments = node["attachments"].as<std::vector<Attachment>>();

    if (node["msaa_samples"]) {
      if (auto samples = node["msaa_samples"].as<std::string>("1x");
          samples == "1x" || samples == "1")
        rhs.msaa_samples = VK_SAMPLE_COUNT_1_BIT;
      else if (samples == "2x" || samples == "2")
        rhs.msaa_samples = VK_SAMPLE_COUNT_2_BIT;
      else if (samples == "4x" || samples == "4")
        rhs.msaa_samples = VK_SAMPLE_COUNT_4_BIT;
      else if (samples == "8x" || samples == "8")
        rhs.msaa_samples = VK_SAMPLE_COUNT_8_BIT;
      else if (samples == "16x" || samples == "16")
        rhs.msaa_samples = VK_SAMPLE_COUNT_16_BIT;
      else if (samples == "32x" || samples == "32")
        rhs.msaa_samples = VK_SAMPLE_COUNT_32_BIT;
      else if (samples == "64x" || samples == "64")
        rhs.msaa_samples = VK_SAMPLE_COUNT_64_BIT;
      else {
        log_error("Invalid sample count specified: " + samples);
        return false;
      }
    }

    return true;
  }
};

} // namespace YAML