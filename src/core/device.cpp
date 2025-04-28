#include "core/device.hpp"

#include "core/allocator.hpp"
#include "core/instance.hpp"

#include <VkBootstrap.h>
#include <cassert>

Device::Device(const Core::Instance& instance, const vkb::Device& dev)
  : device(dev)
  , allocator(std::make_unique<Allocator>(instance, *this))
{
}

auto
Device::create(const Core::Instance& instance, const VkSurfaceKHR& surface)
  -> Device
{
  vkb::PhysicalDeviceSelector selector{ instance.vkb() };
  auto phys_result = selector.set_surface(surface)
                       .set_minimum_version(1, 3)
                       .require_dedicated_transfer_queue()
                       .select();
  if (!phys_result) {
    assert(false && "Failed to select physical device");
  }

  vkb::DeviceBuilder builder{ phys_result.value() };
  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
    .pNext = nullptr,
    .dynamicRendering = VK_TRUE,
  };

  builder.add_pNext(&dynamic_rendering);
  auto dev_result = builder.build();
  if (!dev_result) {
    assert(false && "Failed to create device");
  }

  return Device(instance, dev_result.value());
}
auto
Device::graphics_queue() const -> VkQueue
{
  auto queue_result = device.get_queue(vkb::QueueType::graphics);
  if (!queue_result) {
    assert(false && "Failed to get graphics queue");
  }
  return queue_result.value();
}

auto
Device::get_max_sample_count(VkSampleCountFlags desired_flags) const
  -> VkSampleCountFlagBits
{
  if (!props) {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(device.physical_device.physical_device, &p);
    props = p;
  }

  // device’s full supported mask
  auto available_counts = props->limits.framebufferColorSampleCounts;

  // pick the highest bit in a mask
  static constexpr auto select_max =
    [](VkSampleCountFlags f) -> VkSampleCountFlagBits {
    if (f & VK_SAMPLE_COUNT_64_BIT)
      return VK_SAMPLE_COUNT_64_BIT;
    if (f & VK_SAMPLE_COUNT_32_BIT)
      return VK_SAMPLE_COUNT_32_BIT;
    if (f & VK_SAMPLE_COUNT_16_BIT)
      return VK_SAMPLE_COUNT_16_BIT;
    if (f & VK_SAMPLE_COUNT_8_BIT)
      return VK_SAMPLE_COUNT_8_BIT;
    if (f & VK_SAMPLE_COUNT_4_BIT)
      return VK_SAMPLE_COUNT_4_BIT;
    if (f & VK_SAMPLE_COUNT_2_BIT)
      return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
  };

  // intersect desired with what’s actually available
  auto supported = desired_flags & available_counts;

  // if any bits remain, pick the top one; otherwise fall back to the highest
  // device-supported
  return supported ? select_max(supported) : select_max(available_counts);
}

auto
Device::get_timestamp_period() const -> double
{
  if (!props) {
    props = VkPhysicalDeviceProperties{};
    vkGetPhysicalDeviceProperties(device.physical_device.physical_device,
                                  &props.value());
  }
  return props->limits.timestampPeriod;
}

auto
Device::graphics_queue_family_index() const -> uint32_t
{
  auto queue_result = device.get_queue_index(vkb::QueueType::graphics);
  if (!queue_result) {
    assert(false && "Failed to get graphics queue family index");
  }
  return queue_result.value();
}

auto
Device::transfer_queue_family_index() const -> uint32_t
{
  auto queue_result = device.get_queue_index(vkb::QueueType::transfer);
  if (!queue_result) {
    assert(false && "Failed to get transfer queue family index");
  }
  return queue_result.value();
}

auto
Device::compute_queue() const -> VkQueue
{
  auto queue_result = device.get_queue(vkb::QueueType::compute);
  if (!queue_result) {
    assert(false && "Failed to get compute queue");
  }
  return queue_result.value();
}

auto
Device::compute_queue_family_index() const -> uint32_t
{
  auto queue_result = device.get_queue_index(vkb::QueueType::compute);
  if (!queue_result) {
    assert(false && "Failed to get compute queue family index");
  }
  return queue_result.value();
}

auto
Device::get_queue_family_index(VkQueue queue) const -> std::uint32_t
{
  if (queue == graphics_queue())
    return graphics_queue_family_index();
  if (queue == compute_queue())
    return compute_queue_family_index();
  if (queue == transfer_queue())
    return transfer_queue_family_index();

  assert(false && "Queue not found in device");
  return std::numeric_limits<std::uint32_t>::max();
}

auto
Device::create_one_time_command_buffer() const
  -> std::tuple<VkCommandBuffer, VkCommandPool>
{
  VkCommandPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    .queueFamilyIndex = transfer_queue_family_index(),
  };

  VkCommandPool command_pool{};
  vkCreateCommandPool(get_device(), &pool_info, nullptr, &command_pool);

  VkCommandBufferAllocateInfo alloc_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  VkCommandBuffer command_buffer{};
  vkAllocateCommandBuffers(get_device(), &alloc_info, &command_buffer);

  VkCommandBufferBeginInfo begin_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr,
  };
  vkBeginCommandBuffer(command_buffer, &begin_info);

  return { command_buffer, command_pool };
}

auto
Device::flush(VkCommandBuffer command_buffer, VkCommandPool pool) const -> void
{
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = nullptr,
    .waitSemaphoreCount = 0,
    .pWaitSemaphores = nullptr,
    .pWaitDstStageMask = nullptr,
    .commandBufferCount = 1,
    .pCommandBuffers = &command_buffer,
    .signalSemaphoreCount = 0,
    .pSignalSemaphores = nullptr,
  };

  vkQueueSubmit(transfer_queue(), 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(transfer_queue());
  vkDestroyCommandPool(get_device(), pool, nullptr);
}

auto
Device::transfer_queue() const -> VkQueue
{
  auto queue_result = device.get_queue(vkb::QueueType::transfer);
  if (!queue_result) {
    assert(false && "Failed to get transfer queue");
  }
  return queue_result.value();
}

auto
Device::wait_idle() const -> void
{
  vkDeviceWaitIdle(device.device);
}

auto
Device::destroy() -> void
{
  allocator.reset();
  vkb::destroy_device(device);
}