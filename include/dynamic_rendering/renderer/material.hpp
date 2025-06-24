#pragma once

#include "assets/pointer.hpp"

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/config.hpp"
#include "core/forward.hpp"
#include "core/util.hpp"
#include "pipeline/compute_pipeline_factory.hpp"
#include "pipeline/pipeline_factory.hpp"
#include "renderer/material_bindings.hpp"
#include "renderer/material_data.hpp"

#include <vulkan/vulkan.h>

struct MaterialError
{
  std::string message;

  enum class Code : std::uint8_t
  {
    pipeline_error,
    descriptor_allocation_failed,
    descriptor_layout_failed,
    pool_creation_failed,
    unknown_error
  };
  Code code{ Code::unknown_error };

  std::optional<PipelineError> inner_pipeline_error;
};

class Material
{
public:
  static auto create(const Device&, std::string_view name)
    -> std::expected<Assets::Pointer<Material>, MaterialError>;
  auto initialise(const Device&, const PipelineBlueprint&) -> bool;
  auto initialise(const Device&, std::string_view) -> bool;

  Material() = default;
  ~Material();

  auto upload(std::string_view name, const GPUBuffer* buffer) -> void;
  auto upload(const std::string_view name, const GPUBuffer& buffer) -> void
  {
    return upload(name, &buffer);
  }
  auto upload(const std::string_view name,
              const Assets::Pointer<GPUBuffer>& buffer) -> void
  {
    return upload(name, buffer.get());
  }
  auto upload(const std::string_view name,
              const std::unique_ptr<GPUBuffer>& buffer) -> void
  {
    return upload(name, buffer.get());
  }
  auto upload(std::string_view name, const Image* image) -> void;
  auto upload(const std::string_view name, const Image& image) -> void
  {
    return upload(name, &image);
  }
  auto upload(const std::string_view name, const Assets::Pointer<Image>& image)
    -> void
  {
    return upload(name, image.get());
  }

  auto use_albedo_map(const bool val = true)
  {
    material_data.set_has_albedo_texture(val);
  };
  auto use_normal_map(const bool val = true)
  {
    material_data.set_has_normal_map(val);
  }
  auto use_metallic_map(const bool val = true)
  {
    material_data.set_has_metallic_map(val);
  }
  auto use_roughness_map(const bool val = true)
  {
    material_data.set_has_roughness_map(val);
  }
  auto use_ao_map(const bool val = true) { material_data.set_has_ao_map(val); }
  auto use_emissive_map(const bool val = true)
  {
    material_data.set_has_emissive_map(val);
  }

  auto get_pipeline() const -> const CompiledPipeline& { return *pipeline; }
  auto get_descriptor_set(const std::uint32_t frame_index) const -> const auto&
  {
    return descriptor_sets[frame_index];
  }

  auto get_descriptor_set_layout(const std::uint32_t set) const
    -> VkDescriptorSetLayout
  {
    return set < descriptor_set_layouts.size() ? descriptor_set_layouts[set]
                                               : VK_NULL_HANDLE;
  }

  auto invalidate(const Image* image) -> void
  {
    invalidate(std::span{ &image, 1 });
  }
  auto invalidate(std::span<const Image*> images) -> void;
  auto reload(const PipelineBlueprint&) -> void;
  auto prepare_for_rendering(std::uint32_t frame_index)
    -> const VkDescriptorSet&;

  struct PushConstantInformation
  {
    VkShaderStageFlagBits stage;
    std::uint32_t offset;
    std::uint32_t size;
    const void* pointer;
  };
  [[nodiscard]] auto generate_push_constant_data() const
    -> PushConstantInformation;

  auto get_material_data() -> MaterialData& { return material_data; }

private:
  string_hash_map<std::unique_ptr<GPUBinding>> bindings;
  string_hash_map<std::tuple<std::uint32_t, std::uint32_t>> binding_info;
  frame_array<std::unordered_set<std::string>> per_frame_dirty_flags;

  const Device* device{ nullptr };
  frame_array<VkDescriptorSet> descriptor_sets{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
  VkDescriptorPool descriptor_pool{};
  bool destroyed{ false };

  std::unique_ptr<CompiledPipeline> pipeline{ nullptr };
  std::size_t pipeline_hash{ 0 };

  MaterialData material_data;

  Material(const Device&,
           frame_array<VkDescriptorSet>&&,
           std::vector<VkDescriptorSetLayout>&&,
           VkDescriptorPool,
           std::unique_ptr<CompiledPipeline>,
           string_hash_map<std::tuple<std::uint32_t, std::uint32_t>>&&);

  auto destroy() -> void;

  static inline std::unique_ptr<IPipelineFactory> pipeline_factory{ nullptr };
  static inline std::unique_ptr<IPipelineFactory> compute_pipeline_factory{
    nullptr
  };

  auto rebuild_pipeline(const PipelineBlueprint&)
    -> std::expected<std::unique_ptr<CompiledPipeline>, MaterialError>;
  auto upload_storage_image(std::string_view, const Image*) -> void;

  auto upload_default_textures() -> void;

  static auto create(const Device&, const PipelineBlueprint&)
    -> std::expected<std::unique_ptr<Material>, MaterialError>;
};
