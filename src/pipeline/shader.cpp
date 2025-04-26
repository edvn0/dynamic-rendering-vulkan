#include "shader.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "device.hpp"

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

auto
read_spirv_file(const std::string& path) -> std::vector<std::uint32_t>
{
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    assert(!file.fail() && "Failed to open SPIR-V file");
  }

  const auto size = static_cast<std::size_t>(file.tellg());
  if (size % 4 != 0) {
    assert(size % 4 == 0 && "SPIR-V file size is not a multiple of 4");
  }

  std::vector<std::uint32_t> buffer(size / 4);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(buffer.data()), size);
  return buffer;
}

} // namespace

auto
Shader::load_binary(const std::string_view file_path,
                    std::vector<std::uint32_t>& output) -> bool
{
  namespace fs = std::filesystem;
  auto base_path = fs::current_path() / fs::path("assets") /
                   fs::path("shaders") / fs::path(file_path);

  auto spirv = read_spirv_file(base_path.string());
  output.resize(spirv.size());
  std::memcpy(
    output.data(), spirv.data(), spirv.size() * sizeof(std::uint32_t));
  return true;
}

auto
Shader::load_binary(const std::string_view file_path,
                    std::vector<std::uint8_t>& output) -> bool
{
  namespace fs = std::filesystem;
  auto base_path = fs::current_path() / fs::path("assets") /
                   fs::path("shaders") / fs::path(file_path);

  auto spirv = read_spirv_file(base_path.string());
  output.resize(spirv.size() * sizeof(std::uint32_t));
  std::memcpy(output.data(), spirv.data(), output.size());
  return true;
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
  -> std::unique_ptr<Shader>
{
  auto shader = std::unique_ptr<Shader>(new Shader(device));
  for (const auto& info : stages)
    shader->load_stage(info);
  return shader;
}

auto
Shader::load_stage(const ShaderStageInfo& info) -> void
{
  std::vector<std::uint32_t> spirv{};
  if (!info.empty) {
    namespace fs = std::filesystem;
    auto base_path = fs::current_path() / fs::path("assets") /
                     fs::path("shaders") / fs::path(info.filepath);
    spirv = read_spirv_file(base_path.string());
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
      VK_SUCCESS)
    assert(false && "Failed to create shader module");

  spirv_cache_[info.stage] = std::move(spirv);
  modules_[info.stage] = shader_module;
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
