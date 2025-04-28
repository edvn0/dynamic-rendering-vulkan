#include "material_bindings.hpp"

#include "gpu_buffer.hpp"
#include "image.hpp"

auto
BufferBinding::write_descriptor(uint32_t frame_index, VkDescriptorSet& set)
  -> VkWriteDescriptorSet
{
  auto& info = get_buffer_info(frame_index);
  info = VkDescriptorBufferInfo{
    .buffer = buffer->get(),
    .offset = 0,
    .range = VK_WHOLE_SIZE,
  };

  return VkWriteDescriptorSet{
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = set,
    .dstBinding = get_binding(),
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = get_type(),
    .pImageInfo = nullptr,
    .pBufferInfo = &info,
    .pTexelBufferView = nullptr,
  };
}

auto
ImageBinding::write_descriptor(uint32_t frame_index, VkDescriptorSet& set)
  -> VkWriteDescriptorSet
{
  auto& info = get_buffer_info(frame_index);
  info = image->get_descriptor_info();

  return VkWriteDescriptorSet{
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = set,
    .dstBinding = get_binding(),
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = get_type(),
    .pImageInfo = &info,
    .pBufferInfo = nullptr,
    .pTexelBufferView = nullptr,
  };
}