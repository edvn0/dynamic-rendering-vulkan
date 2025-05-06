#include "pipeline/shader.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "core/device.hpp"
#include "core/fs.hpp"

namespace {

/**
 * @brief Empty fragment shader SPIR-V bytecode.
 *
 * # version 460
 * void main() {}
 *
 * glslangValidator -V empty_frag.frag -o empty_frag.frag.spv
 * xxd -i empty_frag.spv
 *
 * Win 11 x64
 */
constexpr std::array<unsigned char, 180> empty_frag_spv = {
  0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x0b, 0x00, 0x08, 0x00, 0x06,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x47, 0x4c, 0x53,
  0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e, 0x34, 0x35, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0f,
  0x00, 0x05, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6d, 0x61,
  0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00,
  0xcc, 0x01, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6d,
  0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x02, 0x00,
  0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  0x00, 0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x05,
  0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00
};

} // namespace

auto
Shader::load_binary(std::string_view file_path,
                    std::vector<std::uint8_t>& output)
  -> std::expected<void, ShaderError>
{
  std::filesystem::path path = file_path;
  if (path.extension() != ".spv")
    path += ".spv";

  const auto stripped = path.generic_string().starts_with("shaders/")
                          ? path.lexically_relative("shaders")
                          : path;

  auto shader_path = assets_path() / "shaders" / stripped;
  std::vector<std::uint32_t> temp;
  auto result = FS::load_binary(shader_path, temp);
  if (!result)
    return std::unexpected(ShaderError{ .message = result.error().message });

  output.resize(temp.size() * sizeof(std::uint32_t));
  std::memcpy(output.data(), temp.data(), output.size());
  return {};
}

Shader::Shader(const Device& dev)
  : device(&dev)
{
}

Shader::~Shader()
{
  for (const auto& [_, shader_module] : modules_)
    vkDestroyShaderModule(device->get_device(), shader_module, nullptr);
}

auto
Shader::create(const Device& device, const std::vector<ShaderStageInfo>& stages)
  -> std::expected<std::unique_ptr<Shader>, ShaderError>
{
  auto shader = std::unique_ptr<Shader>(new Shader(device));
  for (const auto& info : stages) {
    if (auto err = shader->load_stage(info); !err.has_value())
      return std::unexpected(err.error());
  }
  return shader;
}

auto
Shader::load_stage(const ShaderStageInfo& info)
  -> std::expected<void, ShaderError>
{
  std::vector<std::uint32_t> spirv;

  if (!info.empty) {
    std::filesystem::path path = info.filepath;
    if (path.extension() != ".spv")
      path += ".spv";

    const auto stripped = path.generic_string().starts_with("shaders/")
                            ? path.lexically_relative("shaders")
                            : path;

    const auto full_path = assets_path() / "shaders" / stripped;

    auto result = FS::load_binary(full_path, spirv);
    if (!result) {
      ShaderError error;
      error.message = "Failed to load shader stage: " + full_path.string();
      error.code = ShaderError::Code::file_not_found;
      return std::unexpected(error);
    }
  } else {
    spirv.resize(empty_frag_spv.size() / sizeof(std::uint32_t));
    std::memcpy(spirv.data(), empty_frag_spv.data(), empty_frag_spv.size());
  }

  VkShaderModuleCreateInfo create_info{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .codeSize = spirv.size() * sizeof(std::uint32_t),
    .pCode = spirv.data(),
  };

  VkShaderModule shader_module{};
  if (vkCreateShaderModule(
        device->get_device(), &create_info, nullptr, &shader_module) !=
      VK_SUCCESS) {
    ShaderError error;
    error.message = "Failed to create shader module: " + info.filepath;
    error.code = ShaderError::Code::invalid_shader_module;
    return std::unexpected(error);
  }

  spirv_cache_[info.stage] = std::move(spirv);
  modules_[info.stage] = shader_module;
  return {};
}

auto
Shader::get_module(ShaderStage stage) const -> VkShaderModule
{
  return modules_.at(stage);
}

auto
Shader::get_spirv(ShaderStage stage) const -> const std::vector<std::uint32_t>&
{
  return spirv_cache_.at(stage);
}
