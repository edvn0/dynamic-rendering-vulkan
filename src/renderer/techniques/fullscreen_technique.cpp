#include "dynamic_rendering/renderer/techniques/fullscreen_technique.hpp"
#include "renderer/techniques/fullscreen_technique.hpp"

#include "core/device.hpp"
#include "dynamic_rendering/assets/asset_allocator.hpp"
#include "dynamic_rendering/core/fs.hpp"
#include "dynamic_rendering/renderer/techniques/shadow_gui_technique.hpp"
#include "renderer/descriptor_manager.hpp"
#include "renderer/techniques/point_lights_technique.hpp"
#include "renderer/techniques/shadow_gui_technique.hpp"

#include <tracy/Tracy.hpp>
#include <yaml-cpp/yaml.h>

struct TechniqueParseError
{
  std::string message;
};

auto
parse_fullscreen_technique_yaml(const YAML::Node& root)
  -> std::expected<FullscreenTechniqueDescription, TechniqueParseError>
{
  if (!root["name"] || !root["material"] || !root["output"]) {
    Logger::log_error(
      "Missing required fields: 'name', 'material', or 'output'");
    return std::unexpected(
      TechniqueParseError{ .message = "Missing required fields in root node" });
  }

  FullscreenTechniqueDescription desc;
  desc.name = root["name"].as<std::string>();
  desc.material = root["material"].as<std::string>();

  if (root["bind_point"]) {
    const auto bind_str = root["bind_point"].as<std::string>();
    if (bind_str == "compute") {
      desc.bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    } else if (bind_str == "graphics") {
      desc.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    } else {
      Logger::log_warning("Unknown bind_point '{}', defaulting to graphics",
                          bind_str);
      desc.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }
  } else {
    Logger::log_info("No bind_point specified, defaulting to graphics");
    desc.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
  }

  if (root["inputs"]) {
    for (const auto& input : root["inputs"]) {
      std::string binding;
      std::string source;

      const auto has_binding = input["binding"];
      const auto has_source = input["source"];

      if (has_binding && has_source) {
        binding = input["binding"].as<std::string>();
        source = input["source"].as<std::string>();
      } else if (has_binding) {
        binding = input["binding"].as<std::string>();
        source = binding;
      } else if (has_source) {
        source = input["source"].as<std::string>();
        binding = source;
      } else {
        Logger::log_warning(
          "Skipping input entry due to missing both 'binding' and 'source'");
        continue;
      }

      desc.inputs.push_back(
        { .binding = std::move(binding), .source = std::move(source) });
    }
  }

  const auto& out = root["output"];
  if (!out["binding"] || !out["name"] || !out["extent"] || !out["format"]) {
    Logger::log_error("Missing required fields in 'output': 'binding', 'name', "
                      "'extent', or 'format'");
    return std::unexpected(
      TechniqueParseError{ .message = "Missing fields in output block" });
  }

  FullscreenOutputBinding output;
  output.binding = out["binding"].as<std::string>();
  output.name = out["name"].as<std::string>();
  output.extent = out["extent"].as<std::string>();

  static const string_hash_map<VkFormat> format_map{
    { "RGBA8", VK_FORMAT_R8G8B8A8_UNORM },
    { "RGBA8_UNORM", VK_FORMAT_R8G8B8A8_UNORM },
    { "BGRA8_SRGB", VK_FORMAT_B8G8R8A8_SRGB },
    { "BGRA8_UNORM", VK_FORMAT_B8G8R8A8_UNORM },
    { "RGBA16F", VK_FORMAT_R16G16B16A16_SFLOAT },
    { "RGBA32F", VK_FORMAT_R32G32B32A32_SFLOAT },
    { "RGB32F", VK_FORMAT_R32G32B32_SFLOAT },
    { "RG32F", VK_FORMAT_R32G32_SFLOAT },
    { "R32F", VK_FORMAT_R32_SFLOAT },
    { "R8", VK_FORMAT_R8_UNORM },
    { "R16F", VK_FORMAT_R16_SFLOAT },
    { "R32UI", VK_FORMAT_R32_UINT },
    { "DEPTH16", VK_FORMAT_D16_UNORM },
    { "DEPTH24", VK_FORMAT_X8_D24_UNORM_PACK32 },
    { "DEPTH32", VK_FORMAT_D32_SFLOAT },
    { "DEPTH32_STENCIL8", VK_FORMAT_D32_SFLOAT_S8_UINT },
    { "DEPTH24_STENCIL8", VK_FORMAT_D24_UNORM_S8_UINT }
  };

  const auto format_str = out["format"].as<std::string>();
  const auto fmt_it = format_map.find(format_str);
  if (fmt_it == format_map.end()) {
    Logger::log_error("Unknown format string '{}'", format_str);
    return std::unexpected(
      TechniqueParseError{ .message = "Invalid format: " + format_str });
  }
  output.format = fmt_it->second;

  VkImageUsageFlags usage_flags = 0;
  if (out["usage"]) {
    if (out["usage"].IsSequence()) {
      static const string_hash_map<VkImageUsageFlagBits> usage_map{
        { "TransferSrc", VK_IMAGE_USAGE_TRANSFER_SRC_BIT },
        { "TransferDst", VK_IMAGE_USAGE_TRANSFER_DST_BIT },
        { "Sampled", VK_IMAGE_USAGE_SAMPLED_BIT },
        { "Storage", VK_IMAGE_USAGE_STORAGE_BIT },
        { "Colour", VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT },
        { "Color", VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT },
        { "DepthStencil", VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT },
        { "Transient", VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT },
        { "Input", VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT }
      };

      for (const auto& usage_str : out["usage"]) {
        const auto str = usage_str.as<std::string>();
        const auto it = usage_map.find(str);
        if (it == usage_map.end()) {
          Logger::log_warning("Unknown usage flag '{}', ignoring", str);
          continue;
        }
        usage_flags |= it->second;
      }
    } else {
      usage_flags = static_cast<VkImageUsageFlags>(out["usage"].as<uint32_t>());
      Logger::log_warning(
        "Output usage is not a list; interpreted as raw bitmask: {}",
        usage_flags);
    }
  } else {
    Logger::log_warning(
      "No output usage specified; defaulting to zero usage flags");
  }

  output.usage = usage_flags;
  desc.output = output;

  return desc;
}

template<typename T>
auto
load_from_file(const Device& device,
               DescriptorSetManager& dsm,
               const auto& path) -> Assets::Pointer<IFullscreenTechnique>
{
  const auto root = YAML::LoadFile(path);
  if (auto desc = parse_fullscreen_technique_yaml(root); desc.has_value()) {
    auto data = std::move(desc.value());
    return Assets::make_tracked_as<IFullscreenTechnique, T>(
      device, data.name, dsm, data);
  }
  return nullptr;
}

auto
FullscreenTechniqueFactory::create(std::string_view path,
                                   const Device& device,
                                   DescriptorSetManager& dsm)
  -> Assets::Pointer<IFullscreenTechnique>
{
  const auto actual_path =
    assets_path() / "techniques" / std::format("{}.yaml", path);
  if (!std::filesystem::exists(actual_path)) {
    Logger::log_error("Fullscreen technique file '{}' does not exist",
                      actual_path);
    return nullptr;
  }

  const auto root = YAML::LoadFile(actual_path.generic_string());
  const auto type = root["type"].as<std::string>();

  if (type == "shadow_gui")
    return load_from_file<ShadowGUITechnique>(
      device, dsm, actual_path.string());

  if (type == "point_lights") {
    return load_from_file<PointLightsTechnique>(
      device, dsm, actual_path.string());
  }

  assert(false &&
         "Type could not be matched to any implemented full screen pass");
  return nullptr;
}

auto
FullscreenTechniqueBase::perform(const CommandBuffer&, std::uint32_t) const
  -> void
{
}
