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
                        std::span<const std::byte> data,
                        std::size_t offset = 0ULL) -> void;

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

  template<AdmitsGPUBuffer T, std::size_t N = std::dynamic_extent>
  auto upload_with_offset(std::span<const T, N> data, std::size_t offset_bytes)
    -> void
  {
    const auto required_size = offset_bytes + data.size_bytes();
    if (required_size > current_size)
      recreate(required_size);

    if (!mapped_on_create) {
      upload_to_device_buffer(
        device,
        *this,
        std::span<const std::byte>{
          std::bit_cast<const std::byte*>(data.data()), data.size_bytes() },
        offset_bytes);
      return;
    }

    void* mapped = persistent_ptr;
    if (!mapped) {
      vmaMapMemory(device.get_allocator().get(), allocation, &mapped);
    }

    std::memcpy(static_cast<std::byte*>(mapped) + offset_bytes,
                data.data(),
                data.size_bytes());

    if (!persistent_ptr) {
      vmaUnmapMemory(device.get_allocator().get(), allocation);
    }
  }

  template<AdmitsGPUBuffer T, std::size_t N = std::dynamic_extent>
  auto upload_with_offset(std::span<T, N> data, std::size_t offset_bytes)
    -> void
  {
    upload_with_offset(std::span<const T, N>{ data }, offset_bytes);
  }

  template<AdmitsGPUBuffer T>
  auto read(std::size_t offset_bytes, T& user_allocated) -> bool
  {
    if (!mapped_on_create)
      return false;

    void* mapped = persistent_ptr;
    if (!mapped)
      return false;

    if (offset_bytes + sizeof(T) > current_size)
      return false;

    std::memcpy(&user_allocated,
                static_cast<const std::byte*>(mapped) + offset_bytes,
                sizeof(T));
    return true;
  }
  auto get() const -> const VkBuffer& { return buffer; }

  ~GPUBuffer()
  {
    if (buffer)
      vmaDestroyBuffer(device.get_allocator().get(), buffer, allocation);
  }

private:
  auto recreate(size_t size) -> void;

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
