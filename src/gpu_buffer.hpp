#pragma once

#include "device.hpp"
#include "vulkan/vulkan.h"

#include <span>

class GpuBuffer {
public:
  GpuBuffer(const Device &device, VkBufferUsageFlags usage,
            bool mapped_on_create = false)
      : device(device), usage_flags(usage), mapped_on_create(mapped_on_create) {
  }

  template <typename T>
  void upload(const std::span<const T> data)
    requires(std::is_trivially_copyable_v<T> &&
             std::is_trivially_destructible_v<T>)
  {
    const auto required_size = data.size_bytes();

    if (required_size > current_size)
      recreate(required_size);

    void *mapped = nullptr;
    if (mapped_on_create && persistent_ptr) {
      mapped = persistent_ptr;
    } else {
      vmaMapMemory(device.get_allocator().get(), allocation, &mapped);
    }

    std::memcpy(mapped, data.data(), required_size);

    if (!mapped_on_create)
      vmaUnmapMemory(device.get_allocator().get(), allocation);
  }

  auto get() const -> const auto & { return buffer; }

  ~GpuBuffer() {
    if (mapped_on_create && persistent_ptr)
      vmaUnmapMemory(device.get_allocator().get(), allocation);
    if (buffer)
      vmaDestroyBuffer(device.get_allocator().get(), buffer, allocation);
  }

private:
  auto recreate(size_t size) -> void {
    if (buffer) {
      if (mapped_on_create && persistent_ptr)
        vmaUnmapMemory(device.get_allocator().get(), allocation);
      vmaDestroyBuffer(device.get_allocator().get(), buffer, allocation);
    }

    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo alloc_info{
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    if (mapped_on_create) {
      alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
      alloc_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    VmaAllocationInfo alloc_info_result{};
    vmaCreateBuffer(device.get_allocator().get(), &buffer_info, &alloc_info,
                    &buffer, &allocation,
                    mapped_on_create ? &alloc_info_result : nullptr);

    if (mapped_on_create)
      persistent_ptr = alloc_info_result.pMappedData;

    current_size = size;
  }

  VkBuffer buffer{};
  VmaAllocation allocation{};
  void *persistent_ptr{};
  const Device &device;
  VkBufferUsageFlags usage_flags{};
  bool mapped_on_create{};
  size_t current_size{};
};
