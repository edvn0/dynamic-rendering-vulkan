#pragma once

#include "blueprint_configuration.hpp"

#include <cassert>
#include <yaml-cpp/yaml.h>

namespace YAML {

inline void
set_default_vertex_input(PipelineBlueprint& rhs)
{
  rhs.bindings = {
    { .binding = 0, .stride = 32, .input_rate = VK_VERTEX_INPUT_RATE_VERTEX },
    { .binding = 1, .stride = 48, .input_rate = VK_VERTEX_INPUT_RATE_INSTANCE },
  };

  rhs.attributes = {
    { .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = 0 },
    { .location = 1,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = 12 },
    { .location = 2,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = 24 },
    { .location = 3,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = 0 },
    { .location = 4,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = 16 },
    { .location = 5,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = 32 },
  };
}

template<>
struct convert<VkFormat>
{
  static Node encode(const VkFormat& rhs)
  {
    switch (rhs) {
      case VK_FORMAT_B8G8R8A8_SRGB:
        return Node("b8g8r8a8_srgb");
      case VK_FORMAT_R8G8B8A8_UNORM:
        return Node("r8g8b8a8_unorm");
      case VK_FORMAT_D32_SFLOAT:
        return Node("d32_sfloat");
      case VK_FORMAT_D24_UNORM_S8_UINT:
        return Node("d24_unorm_s8");
      default:
        assert(false && "Unsupported format");
    }
    return Node{};
  }

  static bool decode(const Node& node, VkFormat& rhs)
  {
    const auto format_str = node.as<std::string>();
    if (format_str == "b8g8r8a8_srgb")
      rhs = VK_FORMAT_B8G8R8A8_SRGB;
    else if (format_str == "r8g8b8a8_unorm")
      rhs = VK_FORMAT_R8G8B8A8_UNORM;
    else if (format_str == "d32_sfloat")
      rhs = VK_FORMAT_D32_SFLOAT;
    else if (format_str == "d24_unorm_s8")
      rhs = VK_FORMAT_D24_UNORM_S8_UINT;
    else
      assert(false && "Unsupported format");

    return true;
  }
};

template<>
struct convert<ShaderStageInfo>
{
  static bool decode(const Node& node, ShaderStageInfo& info)
  {
    auto stage = node["stage"].as<std::string>();
    info.filepath = node["path"].as<std::string>();

    if (stage == "vertex")
      info.stage = ShaderStage::vertex;
    else if (stage == "fragment")
      info.stage = ShaderStage::fragment;
    else if (stage == "compute")
      info.stage = ShaderStage::compute;
    else if (stage == "raygen")
      info.stage = ShaderStage::raygen;
    else if (stage == "closest_hit")
      info.stage = ShaderStage::closest_hit;
    else if (stage == "miss")
      info.stage = ShaderStage::miss;
    else
      throw std::runtime_error("Invalid shader stage: " + stage);

    return true;
  }
};

template<>
struct convert<Attachment>
{
  static bool decode(const Node& node, Attachment& out)
  {
    const auto format_str = node["format"].as<std::string>();
    if (format_str == "b8g8r8a8_srgb")
      out.format = VK_FORMAT_B8G8R8A8_SRGB;
    if (format_str == "b8g8r8a8_unorm")
      out.format = VK_FORMAT_B8G8R8A8_UNORM;
    else if (format_str == "r8g8b8a8_unorm")
      out.format = VK_FORMAT_R8G8B8A8_UNORM;
    else if (format_str == "d32_sfloat")
      out.format = VK_FORMAT_D32_SFLOAT;
    else if (format_str == "d24_unorm_s8")
      out.format = VK_FORMAT_D24_UNORM_S8_UINT;
    else
      assert(false && "Unsupported format");

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

    const auto& fmt = node["format"].as<std::string>();
    if (fmt == "vec2")
      rhs.format = VK_FORMAT_R32G32_SFLOAT;
    else if (fmt == "vec3")
      rhs.format = VK_FORMAT_R32G32B32_SFLOAT;
    else if (fmt == "vec4")
      rhs.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    else
      assert(false && "Unsupported format");

    return true;
  }
};

template<>
struct convert<PipelineBlueprint>
{
  static bool decode(const Node& node, PipelineBlueprint& rhs)
  {
    rhs.name = node["name"].as<std::string>();
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
      auto cm = node["rasterization"]["cull_mode"].as<std::string>("back");
      rhs.cull_mode = cm == "none"    ? VK_CULL_MODE_NONE
                      : cm == "front" ? VK_CULL_MODE_FRONT_BIT
                                      : VK_CULL_MODE_BACK_BIT;

      auto pm = node["rasterization"]["polygon_mode"].as<std::string>("fill");
      rhs.polygon_mode = pm == "line"    ? VK_POLYGON_MODE_LINE
                         : pm == "point" ? VK_POLYGON_MODE_POINT
                                         : VK_POLYGON_MODE_FILL;
    }

    if (node["blend"])
      rhs.blend_enable = node["blend"]["enable"].as<bool>(false);

    if (node["depth_stencil"]) {
      auto& depth_stencil = node["depth_stencil"];
      rhs.depth_test = depth_stencil["depth_test"].as<bool>(false);
      rhs.depth_write = depth_stencil["depth_write"].as<bool>(false);

      if (depth_stencil["format"])
        rhs.depth_attachment =
          Attachment{ .format = depth_stencil["format"].as<VkFormat>() };

      if (depth_stencil["compare_op"]) {
        const auto cmp = depth_stencil["compare_op"].as<std::string>();
        if (cmp == "less")
          rhs.depth_compare_op = VK_COMPARE_OP_LESS;
        else if (cmp == "less_equal")
          rhs.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        else if (cmp == "greater")
          rhs.depth_compare_op = VK_COMPARE_OP_GREATER;
        else if (cmp == "greater_equal")
          rhs.depth_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;
        else if (cmp == "always")
          rhs.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        else if (cmp == "never")
          rhs.depth_compare_op = VK_COMPARE_OP_NEVER;
        else if (cmp == "equal")
          rhs.depth_compare_op = VK_COMPARE_OP_EQUAL;
        else if (cmp == "not_equal")
          rhs.depth_compare_op = VK_COMPARE_OP_NOT_EQUAL;
        else
          throw std::runtime_error("Unsupported depth compare_op: " + cmp);
      }

      if (!rhs.depth_test && rhs.depth_write)
        assert(false &&
               "Invalid pipeline config: depth_write requires depth_test");

      if (depth_stencil["compare_op"] && !rhs.depth_test)
        assert(false &&
               "Warning: Using depth compare_op without depth_test may lead to "
               "undefined behavior");

      if ((rhs.depth_compare_op == VK_COMPARE_OP_GREATER ||
           rhs.depth_compare_op == VK_COMPARE_OP_GREATER_OR_EQUAL) &&
          !rhs.depth_write)
        assert(false && "Warning: Using reverse-Z compare_op without "
                        "depth_write may lead to z-fighting");
    }

    if (node["attachments"])
      rhs.attachments = node["attachments"].as<std::vector<Attachment>>();
    return true;
  }
};

} // namespace YAML
