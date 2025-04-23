#pragma once

#include "blueprint_configuration.hpp"

#include <yaml-cpp/yaml.h>

namespace YAML {

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
      throw std::runtime_error("Unsupported format: " + fmt);

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
    rhs.bindings =
      node["vertex_input"]["bindings"].as<std::vector<VertexBinding>>();
    rhs.attributes =
      node["vertex_input"]["attributes"].as<std::vector<VertexAttribute>>();

    auto cm = node["rasterization"]["cull_mode"].as<std::string>();
    rhs.cull_mode = cm == "none"    ? VK_CULL_MODE_NONE
                    : cm == "front" ? VK_CULL_MODE_FRONT_BIT
                                    : VK_CULL_MODE_BACK_BIT;

    auto pm = node["rasterization"]["polygon_mode"].as<std::string>();
    rhs.polygon_mode = pm == "line"    ? VK_POLYGON_MODE_LINE
                       : pm == "point" ? VK_POLYGON_MODE_POINT
                                       : VK_POLYGON_MODE_FILL;

    rhs.blend_enable = node["blend"]["enable"].as<bool>();
    rhs.depth_test = node["depth_stencil"]["depth_test"].as<bool>();
    rhs.depth_write = node["depth_stencil"]["depth_write"].as<bool>();
    return true;
  }
};

} // namespace YAML
