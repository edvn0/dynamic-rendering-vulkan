#pragma once

#include "core/device.hpp"
#include "core/util.hpp"
#include "debug_utils.hpp"
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

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
            const VkBufferUsageFlags usage,
            const bool mapped_on_create = false,
            const std::string_view name = {})
    : device(device)
    , usage_flags(usage)
    , mapped_on_create(mapped_on_create)
  {
    if (!name.empty()) {
      set_name(name);
    }
  }

  static auto zero_initialise(const Device& device,
                              std::size_t bytes,
                              VkBufferUsageFlags usage,
                              bool mapped_on_create = false,
                              std::string_view name = {})
    -> std::unique_ptr<GPUBuffer>;

  ~GPUBuffer();

  auto get_usage_flags() const -> VkBufferUsageFlags { return usage_flags; }
  auto get() const -> const VkBuffer& { return buffer; }
  auto get_size() const -> const auto& { return current_size; }

  auto set_name(std::string_view name) -> void
  {
    debug_name = std::string(name);
    if (buffer && allocation) {
      set_debug_name(debug_name);
    }
  }
  auto get_name() const -> const auto& { return debug_name; }

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
  auto read_into_with_offset(T& user_allocated, std::size_t offset_bytes)
    -> bool
  {
    if (!mapped_on_create)
      return false;

    void const* mapped = persistent_ptr;
    if (!mapped)
      return false;

    if (offset_bytes + sizeof(T) > current_size)
      return false;

    std::memcpy(&user_allocated,
                static_cast<const std::byte*>(mapped) + offset_bytes,
                sizeof(T));
    return true;
  }

private:
  auto recreate(size_t size) -> void;

  auto set_debug_name(const std::string_view name) -> void
  {
    ::set_debug_name(
      device, reinterpret_cast<uint64_t>(buffer), VK_OBJECT_TYPE_BUFFER, name);
    set_vma_allocation_name(device, allocation, name);
  }

  VkBuffer buffer{};
  VmaAllocation allocation{};
  Pointers::transparent persistent_ptr{};
  const Device& device;
  VkBufferUsageFlags usage_flags{};
  bool mapped_on_create{};
  std::size_t current_size{ 0 };
  std::string debug_name;
};

class IndexBuffer
{
public:
  explicit IndexBuffer(const Device& device,
                       const VkIndexType type = VK_INDEX_TYPE_UINT32,
                       const std::string_view name = {})
    : buffer(device,
             VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
             false,
             name)
    , index_type(type)
  {
  }

  ~IndexBuffer() = default;

  template<AdmitsGPUBuffer T, std::size_t N = std::dynamic_extent>
  auto upload_indices(std::span<T, N> data) -> void
  {
    static_assert(std::is_integral_v<T>);
    static_assert(sizeof(T) == 2 || sizeof(T) == 4);

    count = data.size();

    if constexpr (sizeof(T) == 2)
      index_type = VK_INDEX_TYPE_UINT16;
    else
      index_type = VK_INDEX_TYPE_UINT32;

    buffer.upload(std::span<const T, N>{ data });
  }

  auto get_buffer() const -> const GPUBuffer& { return buffer; }
  auto get_count() const -> std::size_t { return count; }
  auto get_index_type() const -> VkIndexType { return index_type; }
  auto get() const -> const VkBuffer& { return buffer.get(); }

private:
  GPUBuffer buffer;
  std::size_t count{};
  VkIndexType index_type;
};

class VertexBuffer
{
public:
  explicit VertexBuffer(const Device& device,
                        const bool mapped_on_create = false,
                        const std::string_view name = {})
    : buffer(device,
             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
             mapped_on_create,
             name)
  {
  }

  ~VertexBuffer() = default;

  template<AdmitsGPUBuffer T, std::size_t N = std::dynamic_extent>
  auto upload_vertices(std::span<T, N> data) -> void
  {
    buffer.upload(std::span<const T, N>{ data });
  }

  auto get_buffer() const -> const GPUBuffer& { return buffer; }
  auto get() const -> const VkBuffer& { return buffer.get(); }

private:
  GPUBuffer buffer;
};
