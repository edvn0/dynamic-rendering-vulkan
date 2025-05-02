#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

class Device;

struct ShaderError
{
  std::string message;

  enum class Code : std::uint8_t
  {
    file_not_found,
    compilation_error,
    invalid_shader_stage,
    invalid_shader_module,
    invalid_spirv_format,
    unknown_error
  };
  Code code{ Code::unknown_error };
};

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
  static auto create(const Device& device,
                     const std::vector<ShaderStageInfo>& stages)
    -> std::expected<std::unique_ptr<Shader>, ShaderError>;

  auto get_module(ShaderStage stage) const -> VkShaderModule;
  auto get_spirv(ShaderStage stage) const -> const std::vector<std::uint32_t>&;

  ~Shader();

  static auto load_binary(const std::string_view, std::vector<std::uint8_t>&)
    -> std::expected<void, ShaderError>;
  static auto load_binary(const std::string_view, std::vector<std::uint32_t>&)
    -> std::expected<void, ShaderError>;

private:
  explicit Shader(const Device& device);
  auto load_stage(const ShaderStageInfo&) -> std::expected<void, ShaderError>;

  const Device* device{ nullptr };
  std::unordered_map<ShaderStage, VkShaderModule> modules_;
  std::unordered_map<ShaderStage, std::vector<std::uint32_t>> spirv_cache_;
};