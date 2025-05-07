#include "renderer/descriptor_allocator.hpp"

#include "core/device.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image.hpp"

#include <cstring>

DescriptorAllocator::DescriptorAllocator(
  const Device& device,
  std::span<DescriptorBindingMetadata> metadata,
  std::uint32_t image_count)
  : device(&device)
  , image_count(image_count)
  , bindings(metadata.begin(), metadata.end())
{
}

DescriptorAllocator::~DescriptorAllocator()
{
  destroy();
}

auto
DescriptorAllocator::create_layout() -> VkDescriptorSetLayout
{
  std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
  for (const auto& meta : bindings) {
    layout_bindings.push_back({
      .binding = meta.binding,
      .descriptorType = meta.descriptor_type,
      .descriptorCount = 1,
      .stageFlags = meta.stage_flags,
    });
  }

  VkDescriptorSetLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
    .pBindings = layout_bindings.data(),
  };

  VkDescriptorSetLayout layout;
  vkCreateDescriptorSetLayout(
    device->get_device(), &layout_info, nullptr, &layout);
  return layout;
}

auto
DescriptorAllocator::allocate_sets(VkDescriptorSetLayout layout)
  -> std::vector<VkDescriptorSet>
{
  std::vector<VkDescriptorPoolSize> pool_sizes;
  for (const auto& meta : bindings) {
    pool_sizes.push_back({
      .type = meta.descriptor_type,
      .descriptorCount = image_count,
    });
  }

  VkDescriptorPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = image_count,
    .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  };

  vkCreateDescriptorPool(
    device->get_device(), &pool_info, nullptr, &descriptor_pool);

  std::vector<VkDescriptorSetLayout> layouts(image_count, layout);
  std::vector<VkDescriptorSet> sets(image_count);

  VkDescriptorSetAllocateInfo alloc_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = descriptor_pool,
    .descriptorSetCount = image_count,
    .pSetLayouts = layouts.data(),
  };

  vkAllocateDescriptorSets(device->get_device(), &alloc_info, sets.data());
  return sets;
}

auto
DescriptorAllocator::update_sets(std::span<VkDescriptorSet> sets) -> void
{
  const auto total_bindings = bindings.size() * image_count;
  std::vector<VkWriteDescriptorSet> write_sets(total_bindings);
  std::vector<VkDescriptorBufferInfo> buffer_infos(total_bindings);

  for (std::size_t i = 0; i < image_count; ++i) {
    for (std::size_t j = 0; j < bindings.size(); ++j) {
      const auto& meta = bindings[j];
      const std::size_t index = i * bindings.size() + j;

      auto& write = write_sets[index];
      write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = sets[i],
        .dstBinding = meta.binding,
        .descriptorCount = 1,
        .descriptorType = meta.descriptor_type,
      };

      if (meta.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
          meta.buffer) {
        buffer_infos[index] = {
          .buffer = meta.buffer->get(),
          .offset = i * meta.element_size,
          .range = meta.element_size,
        };
        write.pBufferInfo = &buffer_infos[index];
      }
    }
  }

  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<uint32_t>(write_sets.size()),
                         write_sets.data(),
                         0,
                         nullptr);
}

auto
DescriptorAllocator::destroy() -> void
{
  if (descriptor_pool) {
    vkDestroyDescriptorPool(device->get_device(), descriptor_pool, nullptr);
    descriptor_pool = nullptr;
  }
}

auto
DescriptorAllocator::get_metadata() const
  -> const std::vector<DescriptorBindingMetadata>&
{
  return bindings;
}
