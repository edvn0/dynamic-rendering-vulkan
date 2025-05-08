#pragma once

#include "core/config.hpp"
#include "core/forward.hpp"

#include <vulkan/vulkan.h>

class GPUBinding
{
public:
  virtual ~GPUBinding() = default;
  virtual auto write_descriptor(std::uint32_t, VkDescriptorSet&)
    -> VkWriteDescriptorSet = 0;
};

template<typename WriteBufferInfo>
class BaseGPUBinding : public GPUBinding
{
public:
  BaseGPUBinding(const VkDescriptorType type, const std::uint32_t binding)
    : type(type)
    , binding(binding)
  {
    buffer_infos.fill({});
  }
  ~BaseGPUBinding() override = default;

protected:
  auto get_type() -> auto& { return type; }
  auto get_binding() -> auto& { return binding; }
  auto get_buffer_info(std::uint32_t index) -> auto&
  {
    return buffer_infos.at(index);
  }

private:
  VkDescriptorType type;
  std::uint32_t binding{};
  frame_array<WriteBufferInfo> buffer_infos;
};

class BufferBinding final : public BaseGPUBinding<VkDescriptorBufferInfo>
{
public:
  BufferBinding(const GPUBuffer* buf,
                const VkDescriptorType type,
                const std::uint32_t binding)
    : BaseGPUBinding(type, binding)
    , buffer(buf)
  {
  }
  ~BufferBinding() override = default;

  auto get_underlying_buffer() const -> const GPUBuffer* { return buffer; }

  auto write_descriptor(std::uint32_t frame_index, VkDescriptorSet& set)
    -> VkWriteDescriptorSet override;

private:
  const GPUBuffer* buffer;
};

class ImageBinding final : public BaseGPUBinding<VkDescriptorImageInfo>
{
public:
  ImageBinding(const Image* img,
               const VkDescriptorType type,
               const std::uint32_t binding)
    : BaseGPUBinding(type, binding)
    , image(img)
  {
  }
  ~ImageBinding() override = default;

  auto get_underlying_image() const -> const Image* { return image; }

  auto write_descriptor(std::uint32_t frame_index, VkDescriptorSet& set)
    -> VkWriteDescriptorSet override;

private:
  const Image* image;
};