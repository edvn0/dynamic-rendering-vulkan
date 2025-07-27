#include "renderer/material_bindings.hpp"

#include "core/gpu_buffer.hpp"
#include "core/image.hpp"
#include "core/image_array.hpp"

auto
BufferBinding::write_descriptor(uint32_t frame_index,
                                const VkDescriptorSet& set,
                                std::vector<VkWriteDescriptorSet>& output)
  -> void
{
  auto& info = get_buffer_info(frame_index);
  info = VkDescriptorBufferInfo{
    .buffer = buffer->get(),
    .offset = 0,
    .range = VK_WHOLE_SIZE,
  };

  output.push_back(VkWriteDescriptorSet{
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
  });
}

auto
ImageBinding::write_descriptor(uint32_t frame_index,
                               const VkDescriptorSet& set,
                               std::vector<VkWriteDescriptorSet>& output)
  -> void
{
  auto& info = get_buffer_info(frame_index);
  info = image->get_descriptor_info();

  output.push_back(VkWriteDescriptorSet{
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
  });
}

ImageArrayBinding::ImageArrayBinding(const ImageArray* img,
                                     const VkDescriptorType type,
                                     const std::uint32_t binding)
  : BaseGPUBinding(type, binding)
  , image(img)
{
}

auto
ImageArrayBinding::write_descriptor(uint32_t,
                                    const VkDescriptorSet& set,
                                    std::vector<VkWriteDescriptorSet>& output)
  -> void
{
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = nullptr;
  write.dstSet = set;
  write.dstBinding = get_binding();
  write.dstArrayElement = 0;
  write.descriptorCount = 1;
  write.descriptorType = get_type();
  write.pImageInfo = &image->get_image()->get_descriptor_info();
  write.pBufferInfo = nullptr;
  write.pTexelBufferView = nullptr;
  output.push_back(write);
}