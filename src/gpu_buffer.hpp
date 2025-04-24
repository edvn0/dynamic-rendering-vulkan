#pragma once

#include "device.hpp"
#include "vulkan/vulkan.h"

#include <span>
#include <type_traits>
#include <vk_mem_alloc.h>

template<typename T>
concept AdmitsGPUBuffer =
  std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

class GPUBuffer;

auto
upload_to_device_buffer(const Device& device,
                        GPUBuffer& target_buffer,
                        std::span<const std::byte> data) -> void;

class GPUBuffer
{
public:
  GPUBuffer(const Device& device,
            VkBufferUsageFlags usage,
            bool mapped_on_create = false)
    : device(device)
    , usage_flags(usage)
    , mapped_on_create(mapped_on_create)
  {
  }

  template<AdmitsGPUBuffer T, std::size_t N = std::dynamic_extent>
  auto upload(std::span<const T, N> data) -> void
  {

    const auto required_size = data.size_bytes();
    if (required_size > current_size)
      recreate(required_size);

    if (!mapped_on_create) {
      upload_to_device_buffer(
        device,
        *this,
        std::span<const std::byte>{
          reinterpret_cast<const std::byte*>(data.data()), data.size_bytes() });
      return;
    }

    void* mapped = persistent_ptr;
    if (!mapped) {
      vmaMapMemory(device.get_allocator().get(), allocation, &mapped);
    }

    std::memcpy(mapped, data.data(), required_size);

    if (!persistent_ptr) {
      vmaUnmapMemory(device.get_allocator().get(), allocation);
    }
  }
  template<AdmitsGPUBuffer T, std::size_t N = std::dynamic_extent>
  auto upload(std::span<T, N> data) -> void
  {
    upload(std::span<const T, N>{ data });
  }

  auto get() const -> const VkBuffer& { return buffer; }

  ~GPUBuffer()
  {
    if (buffer)
      vmaDestroyBuffer(device.get_allocator().get(), buffer, allocation);
  }

private:
  auto recreate(size_t size) -> void
  {
    if (buffer) {
      if (mapped_on_create && persistent_ptr)
        vmaUnmapMemory(device.get_allocator().get(), allocation);
      vmaDestroyBuffer(device.get_allocator().get(), buffer, allocation);
    }

    VkBufferCreateInfo buffer_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = size,
      .usage = usage_flags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
    };

    VmaAllocationCreateInfo alloc_info{
      .flags = 0,
      .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      .requiredFlags = 0,
      .preferredFlags = 0,
      .memoryTypeBits = 0,
      .pool = nullptr,
      .pUserData = nullptr,
      .priority = 0,
    };

    alloc_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (mapped_on_create) {
      alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VmaAllocationInfo alloc_info_result{};
    vmaCreateBuffer(device.get_allocator().get(),
                    &buffer_info,
                    &alloc_info,
                    &buffer,
                    &allocation,
                    mapped_on_create ? &alloc_info_result : nullptr);

    if (mapped_on_create)
      persistent_ptr = alloc_info_result.pMappedData;

    current_size = size;
  }

  VkBuffer buffer{};
  VmaAllocation allocation{};
  void* persistent_ptr{};
  const Device& device;
  VkBufferUsageFlags usage_flags{};
  bool mapped_on_create{};
  std::size_t current_size{};
};

class IndexBuffer
{
public:
  IndexBuffer(const Device& device,
              VkIndexType index_type = VK_INDEX_TYPE_UINT32)
    : buffer(device,
             VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
             false)
    , index_type(index_type)
  {
  }

  ~IndexBuffer() = default;

  template<AdmitsGPUBuffer T, std::size_t N = std::dynamic_extent>
  auto upload(std::span<const T, N> data) -> void
  {
    count = data.size();
    if (count > std::numeric_limits<std::uint16_t>::max()) {
      index_type = VK_INDEX_TYPE_UINT32;
    }
    buffer.upload(data);
  }

  template<AdmitsGPUBuffer T, std::size_t N = std::dynamic_extent>
  auto upload(std::span<T, N> data) -> void
  {
    upload(std::span<const T, N>{ data });
  }

  auto get() const -> const VkBuffer& { return buffer.get(); }
  auto get_count() const -> std::size_t { return count; }
  auto get_index_type() const -> VkIndexType { return index_type; }

private:
  GPUBuffer buffer;
  std::size_t count{};
  VkIndexType index_type;
};
