#pragma once

#include "core/config.hpp"
#include "core/forward.hpp"
#include "core/gpu_buffer.hpp"

#include <span>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>

struct DescriptorBindingMetadata
{
  uint32_t binding;
  VkDescriptorType descriptor_type;
  VkShaderStageFlags stage_flags;
  std::string_view name;
  GPUBuffer* buffer{ nullptr };
  Image* image{ nullptr };
  std::size_t element_size{ 0 };
};

class Device;

class DescriptorLayoutBuilder
{
public:
  explicit DescriptorLayoutBuilder(std::span<const DescriptorBindingMetadata>);
  [[nodiscard]] auto create_layout(const Device&) const
    -> VkDescriptorSetLayout;
  [[nodiscard]] auto get_pool_sizes(std::uint32_t) const
    -> std::vector<VkDescriptorPoolSize>;
  [[nodiscard]] auto get_bindings() const
    -> const std::vector<DescriptorBindingMetadata>&;

private:
  std::vector<DescriptorBindingMetadata> bindings;
};

class DescriptorSetManager
{
public:
  DescriptorSetManager(const Device&, DescriptorLayoutBuilder&&);
  ~DescriptorSetManager();

  /// <summary>Uses the pointers as identifiers to write.</summary>
  auto allocate_sets(std::span<GPUBuffer*>, std::span<Image*>) -> void;

  auto destroy() const -> void;
  auto resize(std::uint32_t, std::uint32_t) -> void;

  [[nodiscard]] auto get_layout() const -> VkDescriptorSetLayout;
  [[nodiscard]] auto get_metadata() const
    -> const std::vector<DescriptorBindingMetadata>&;

  [[nodiscard]] auto get_set(const std::uint32_t index) const -> const auto&
  {
    return descriptor_sets[index];
  }

private:
  const Device* device{ nullptr };
  std::vector<DescriptorBindingMetadata> bindings{};
  frame_array<VkDescriptorSet> descriptor_sets{};
  VkDescriptorSetLayout descriptor_set_layout{};
  VkDescriptorPool descriptor_pool{};

  auto update_sets(std::span<VkDescriptorSet>) const -> void;
};
