#include "core/gpu_buffer.hpp"

#include "assets/asset_allocator.hpp"
#include "assets/manager.hpp"
#include "core/allocator.hpp"

auto
align_buffer_offset(std::size_t offset, std::size_t alignment) -> std::size_t
{
  return (offset + alignment - 1) & ~(alignment - 1);
}

auto
get_aligned_buffer_size(const Device& device,
                        std::size_t requested_size,
                        VkBufferUsageFlags usage_flags) -> std::size_t
{
  std::size_t alignment = 1;

  const auto& limits = device.get_physical_device_properties().limits;

  if (usage_flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
    alignment = std::max(alignment, limits.minUniformBufferOffsetAlignment);
  }

  if (usage_flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
    alignment = std::max(alignment, limits.minStorageBufferOffsetAlignment);
  }

  if (usage_flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ||
      usage_flags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) {
    alignment = std::max(alignment, limits.minTexelBufferOffsetAlignment);
  }

  return align_buffer_offset(requested_size, alignment);
}

auto
get_aligned_buffer_size(const Device& device,
                        std::size_t requested_size,
                        const GPUBuffer& buffer) -> std::size_t
{
  return get_aligned_buffer_size(
    device, requested_size, buffer.get_usage_flags());
}

auto
get_buffer_alignment(const Device& device, VkBufferUsageFlags usage_flags)
  -> std::size_t
{
  std::size_t alignment = 1;
  const auto& limits = device.get_physical_device_properties().limits;

  if (usage_flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
    alignment = std::max(alignment, limits.minUniformBufferOffsetAlignment);
  }

  if (usage_flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
    alignment = std::max(alignment, limits.minStorageBufferOffsetAlignment);
  }

  if (usage_flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ||
      usage_flags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) {
    alignment = std::max(alignment, limits.minTexelBufferOffsetAlignment);
  }

  return alignment;
}

auto
upload_to_device_buffer(const Device& device,
                        GPUBuffer& target_buffer,
                        std::span<const std::byte> data,
                        const std::size_t offset) -> void
{
  const auto aligned_offset = align_buffer_offset(
    offset, get_buffer_alignment(device, target_buffer.get_usage_flags()));

  GPUBuffer staging{
    device,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    true,
    "Staging buffer (GPU Buffer)",
  };
  staging.upload(data);

  const VkBufferCopy copy_region{
    .srcOffset = 0,
    .dstOffset = aligned_offset,
    .size = data.size_bytes(),
  };

  const OneTimeCommand command{ device };
  vkCmdCopyBuffer(
    *command, staging.get(), target_buffer.get(), 1, &copy_region);
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
GPUBuffer::recreate(std::size_t size) -> void
{
  assert(!debug_name.empty());

  auto aligned_size = get_aligned_buffer_size(device, size, usage_flags);

  if (size != aligned_size) {
    Logger::log_info("Buffer with name {} was aligned from {} to {} bytes",
                     debug_name,
                     size,
                     aligned_size);
  }

  if (buffer) {
    if (mapped_on_create && persistent_ptr)
      vmaUnmapMemory(device.get_allocator().get(), allocation);
    vmaDestroyBuffer(device.get_allocator().get(), buffer, allocation);
  }

  VkBufferCreateInfo buffer_info{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = aligned_size,
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

  current_size = aligned_size;

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
