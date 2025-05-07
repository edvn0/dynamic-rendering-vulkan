#pragma once

#include "core/forward.hpp"

#include <cstdint>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

struct DescriptorBindingMetadata
{
  uint32_t binding;
  VkDescriptorType descriptor_type;
  VkShaderStageFlags stage_flags;
  std::string_view name;
  GPUBuffer* buffer{ nullptr };
  std::size_t element_size{ 0 };
};

class Device;

class DescriptorAllocator
{
public:
  DescriptorAllocator(const Device&,
                      std::span<DescriptorBindingMetadata>,
                      std::uint32_t);

  ~DescriptorAllocator();

  auto create_layout() -> VkDescriptorSetLayout;
  auto allocate_sets(VkDescriptorSetLayout) -> std::vector<VkDescriptorSet>;
  auto update_sets(std::span<VkDescriptorSet>) -> void;
  auto destroy() -> void;

  auto get_metadata() const -> const std::vector<DescriptorBindingMetadata>&;

private:
  const Device* device;
  std::uint32_t image_count;
  std::vector<DescriptorBindingMetadata> bindings;
  VkDescriptorPool descriptor_pool{ nullptr };
};
