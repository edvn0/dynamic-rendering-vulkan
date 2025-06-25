#include "core/gpu_buffer.hpp"

#include "assets/asset_allocator.hpp"
#include "assets/manager.hpp"
#include "core/allocator.hpp"

auto
upload_to_device_buffer(const Device& device,
                        GPUBuffer& target_buffer,
                        std::span<const std::byte> data,
                        std::size_t offset) -> void
{
  GPUBuffer staging{ device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true };
  staging.upload(data);

  VkBufferCopy copy_region{
    .srcOffset = 0,
    .dstOffset = offset,
    .size = data.size_bytes(),
  };

  auto&& [command_buffer, command_pool] =
    device.create_one_time_command_buffer();
  vkCmdCopyBuffer(
    command_buffer, staging.get(), target_buffer.get(), 1, &copy_region);

  device.flush(command_buffer, command_pool);
}

auto
GPUBuffer::zero_initialise(const Device& device,
                           const std::size_t bytes,
                           const VkBufferUsageFlags usage,
                           const bool mapped_on_create,
                           const std::string_view name)
  -> std::unique_ptr<GPUBuffer>
{
  auto buffer =
    std::make_unique<GPUBuffer>(device, usage, mapped_on_create, name);

  const auto memory = std::make_unique<std::byte[]>(bytes);
  buffer->upload(std::span(memory.get(), bytes));
  return std::move(buffer);
}

GPUBuffer::~GPUBuffer()
{
  vmaDestroyBuffer(device.get_allocator().get(), buffer, allocation);
}

auto
GPUBuffer::recreate(size_t size) -> void
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

  if (!debug_name.empty()) {
    set_debug_name(debug_name);
  }
}

auto
GPUBuffer::map(Pointers::transparent mapped_data) const -> void
{
  vmaMapMemory(device.get_allocator().get(), allocation, &mapped_data);
}

auto
GPUBuffer::unmap() const -> void
{
  vmaUnmapMemory(device.get_allocator().get(), allocation);
}
