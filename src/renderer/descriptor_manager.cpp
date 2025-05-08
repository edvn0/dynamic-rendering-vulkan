#include "renderer/descriptor_manager.hpp"
#include "core/device.hpp"
#include "core/image.hpp"

DescriptorLayoutBuilder::DescriptorLayoutBuilder(
  std::span<const DescriptorBindingMetadata> meta)
  : bindings(meta.begin(), meta.end())
{
}

auto
DescriptorLayoutBuilder::create_layout(const Device& device) const
  -> VkDescriptorSetLayout
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
    device.get_device(), &layout_info, nullptr, &layout);
  return layout;
}

auto
DescriptorLayoutBuilder::get_pool_sizes(const std::uint32_t count) const
  -> std::vector<VkDescriptorPoolSize>
{
  std::vector<VkDescriptorPoolSize> sizes;
  for (const auto& meta : bindings) {
    sizes.push_back({
      .type = meta.descriptor_type,
      .descriptorCount = count,
    });
  }
  return sizes;
}

auto
DescriptorLayoutBuilder::get_bindings() const
  -> const std::vector<DescriptorBindingMetadata>&
{
  return bindings;
}

DescriptorSetManager::DescriptorSetManager(const Device& dev,
                                           DescriptorLayoutBuilder&& builder)
  : device(&dev)
  , bindings(builder.get_bindings())
  , descriptor_set_layout(builder.create_layout(dev))
{
  auto pool_sizes = builder.get_pool_sizes(image_count);

  const VkDescriptorPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = image_count,
    .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  };

  vkCreateDescriptorPool(
    dev.get_device(), &pool_info, nullptr, &descriptor_pool);
}

DescriptorSetManager::~DescriptorSetManager()
{
  destroy();
}

auto
DescriptorSetManager::allocate_sets(std::span<GPUBuffer*> buffers,
                                    std::span<Image*> images) -> void
{
  // Allocate descriptor sets
  std::vector layouts(image_count, descriptor_set_layout);
  const VkDescriptorSetAllocateInfo alloc_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = descriptor_pool,
    .descriptorSetCount = image_count,
    .pSetLayouts = layouts.data(),
  };
  vkAllocateDescriptorSets(
    device->get_device(), &alloc_info, descriptor_sets.data());

  auto divided_by_image_count = [](const auto& val) {
    if (val % image_count != 0)
      assert(false && "Must be a multiple of image_count.");

    return val / image_count;
  };

  for (auto& meta : bindings) {
    if (meta.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
      meta.buffer = nullptr;
      for (auto* buf : buffers) {
        if (buf->get_name() == meta.name) {
          meta.buffer = buf;
          meta.element_size = divided_by_image_count(buf->get_size());
        }
      }
      assert(meta.buffer && "DescriptorSetManager::allocate_sets: no matching "
                            "GPUBuffer for descriptor");
    } else if (meta.descriptor_type ==
               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
      meta.image = nullptr;
      for (auto* img : images) {
        if (img->get_name() == meta.name) {
          meta.image = img;
          break;
        }
      }
      assert(meta.image && "DescriptorSetManager::allocate_sets: no matching "
                           "Image for descriptor");
    }
  }

  update_sets({ descriptor_sets.data(), image_count });
}

auto
DescriptorSetManager::update_sets(const std::span<VkDescriptorSet> sets) const
  -> void
{
  const std::size_t total = bindings.size() * image_count;
  std::vector<VkWriteDescriptorSet> writes(total);
  std::vector<VkDescriptorBufferInfo> buffers(total);
  std::vector<VkDescriptorImageInfo> images(total);

  for (std::size_t i = 0; i < image_count; ++i) {
    for (std::size_t j = 0; j < bindings.size(); ++j) {
      const auto& meta = bindings[j];
      const std::size_t index = i * bindings.size() + j;

      writes[index] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = sets[i],
        .dstBinding = meta.binding,
        .descriptorCount = 1,
        .descriptorType = meta.descriptor_type,
      };

      if (meta.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        buffers[index] = {
          .buffer = meta.buffer->get(),
          .offset = i * meta.element_size,
          .range = meta.element_size,
        };
        writes[index].pBufferInfo = &buffers[index];
      } else if (meta.descriptor_type ==
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        images[index] = {
          .sampler = meta.image->get_sampler(),
          .imageView = meta.image->get_view(),
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        writes[index].pImageInfo = &images[index];
      }
    }
  }

  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<std::uint32_t>(writes.size()),
                         writes.data(),
                         0,
                         nullptr);
}

auto
DescriptorSetManager::destroy() const -> void
{
  if (descriptor_set_layout)
    vkDestroyDescriptorSetLayout(
      device->get_device(), descriptor_set_layout, nullptr);
  if (descriptor_pool)
    vkDestroyDescriptorPool(device->get_device(), descriptor_pool, nullptr);
}

auto
DescriptorSetManager::get_layout() const -> VkDescriptorSetLayout
{
  return descriptor_set_layout;
}

auto
DescriptorSetManager::get_metadata() const
  -> const std::vector<DescriptorBindingMetadata>&
{
  return bindings;
}
