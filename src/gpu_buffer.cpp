#include "gpu_buffer.hpp"

auto
upload_to_device_buffer(const Device& device,
                        GPUBuffer& target_buffer,
                        std::span<const std::byte> data) -> void
{
  GPUBuffer staging{ device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true };

  staging.upload(data);

  VkBufferCopy copy_region{
    .srcOffset = 0,
    .dstOffset = 0,
    .size = data.size_bytes(),
  };

  auto&& [command_buffer, command_pool] =
    device.create_one_time_command_buffer();
  vkCmdCopyBuffer(
    command_buffer, staging.get(), target_buffer.get(), 1, &copy_region);

  device.flush(command_buffer, command_pool);
}