#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

enum class ShaderStage : std::uint8_t
{
  vertex,
  fragment,
  compute,
  raygen,
  closest_hit,
  miss,
  any_hit,
  intersection
};

struct ShaderStageInfo
{
  ShaderStage stage;
  std::string filepath;
  bool empty{ false }; // Support empty fragment shader.
};

class Shader
{
public:
  static auto create(const VkDevice device,
                     const std::vector<ShaderStageInfo>& stages)
    -> std::unique_ptr<Shader>;

  auto get_module(ShaderStage stage) const -> VkShaderModule;
  auto get_spirv(ShaderStage stage) const -> const std::vector<std::uint32_t>&;

  ~Shader();

private:
  Shader(VkDevice device);

  auto load_stage(const ShaderStageInfo& info) -> void;

  VkDevice device_;
  std::unordered_map<ShaderStage, VkShaderModule> modules_;
  std::unordered_map<ShaderStage, std::vector<std::uint32_t>> spirv_cache_;
};