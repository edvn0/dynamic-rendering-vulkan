#include "shader.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>


namespace {

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

auto
to_vk_stage(ShaderStage stage) -> VkShaderStageFlagBits
{
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
    case ShaderStage::any_hit:
      return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case ShaderStage::intersection:
      return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
  }
  return VK_SHADER_STAGE_ALL; // fallback
}

} // namespace

Shader::Shader(VkDevice device)
  : device_(device)
{
}

Shader::~Shader()
{
  for (const auto& [_, module] : modules_)
    vkDestroyShaderModule(device_, module, nullptr);
}

auto
Shader::create(const VkDevice device,
               const std::vector<ShaderStageInfo>& stages)
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
  auto base_path = std::filesystem::path("assets/shaders/") / info.filepath;
  auto spirv = read_spirv_file(base_path.string());

  VkShaderModuleCreateInfo create_info{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .codeSize = spirv.size() * sizeof(std::uint32_t),
    .pCode = spirv.data(),
  };

  VkShaderModule module{};
  if (vkCreateShaderModule(device_, &create_info, nullptr, &module) !=
      VK_SUCCESS)
    throw std::runtime_error("Failed to create shader module: " +
                             info.filepath);

  spirv_cache_[info.stage] = std::move(spirv);
  modules_[info.stage] = module;
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
