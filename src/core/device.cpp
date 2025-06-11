#include "core/device.hpp"

#include "core/allocator.hpp"
#include "core/instance.hpp"
#include "pipeline/blueprint_registry.hpp"

#include <VkBootstrap.h>
#include <cassert>
#include <tracy/Tracy.hpp>

Device::Device(const Core::Instance& instance, const vkb::Device& dev)
  : device(dev)
  , allocator(std::make_unique<Allocator>(instance, *this))
{
  graphics = device.get_queue(vkb::QueueType::graphics).value();
  compute = device.get_queue(vkb::QueueType::compute).value();
  transfer = device.get_queue(vkb::QueueType::transfer).value();
  graphics_family = device.get_queue_index(vkb::QueueType::graphics).value();
  compute_family = device.get_queue_index(vkb::QueueType::compute).value();
  transfer_family = device.get_queue_index(vkb::QueueType::transfer).value();
  assert(graphics != VK_NULL_HANDLE && "Failed to get graphics queue");
  assert(compute != VK_NULL_HANDLE && "Failed to get compute queue");
  assert(transfer != VK_NULL_HANDLE && "Failed to get transfer queue");

  props = VkPhysicalDeviceProperties{};
  vkGetPhysicalDeviceProperties(device.physical_device.physical_device,
                                &props.value());

  {
    ZoneScopedN("Load blueprints");
    blueprint_registry = std::make_unique<BlueprintRegistry>();
    blueprint_registry->load_from_directory("blueprints");
  }
}

auto
Device::create(const Core::Instance& instance, const VkSurfaceKHR& surface)
  -> Device
{

  vkb::PhysicalDeviceSelector selector{ instance.vkb() };
  VkPhysicalDeviceFeatures features{
    .depthClamp = VK_TRUE,
    .depthBiasClamp = VK_TRUE,
  };

  auto phys_result = selector.set_surface(surface)
                       .set_minimum_version(1, 3)
                       .set_required_features(features)
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
  return graphics;
}

auto
Device::graphics_queue_family_index() const -> uint32_t
{
  return graphics_family;
}

auto
Device::transfer_queue_family_index() const -> uint32_t
{
  return transfer_family;
}

auto
Device::compute_queue() const -> VkQueue
{
  return compute;
}

auto
Device::compute_queue_family_index() const -> uint32_t
{
  return compute_family;
}

auto
Device::transfer_queue() const -> VkQueue
{
  return transfer;
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
Device::create_one_time_command_buffer(VkQueue queue) const
  -> std::tuple<VkCommandBuffer, VkCommandPool>
{
  VkCommandPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    .queueFamilyIndex =
      get_queue_family_index(queue ? queue : transfer_queue()),
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
Device::flush(VkCommandBuffer command_buffer,
              VkCommandPool pool,
              VkQueue queue) const -> void
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

  auto chosen = queue ? queue : transfer_queue();
  vkQueueSubmit(chosen, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(chosen);
  vkDestroyCommandPool(get_device(), pool, nullptr);
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
auto
Device::get_max_sample_count(VkSampleCountFlags desired_flags) const
  -> VkSampleCountFlagBits
{
  auto available_counts = props->limits.framebufferColorSampleCounts;

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

  auto supported = desired_flags & available_counts;

  return supported ? select_max(supported) : select_max(available_counts);
}

auto
Device::get_timestamp_period() const -> double
{
  return props->limits.timestampPeriod;
}

auto
Device::allocate_primary_command_buffer(VkCommandPool pool) const
  -> VkCommandBuffer
{
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(device, &alloc_info, &cmd);

  return cmd;
}

auto
Device::allocate_secondary_command_buffer(VkCommandPool pool) const
  -> VkCommandBuffer
{
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(device, &alloc_info, &cmd);

  return cmd;
}

auto
Device::create_resettable_command_pool() const -> VkCommandPool
{
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex =
    graphics_queue_family_index(); // your stored index
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VkCommandPool command_pool;
  vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);
  return command_pool;
}
