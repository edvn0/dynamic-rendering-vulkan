#pragma once

#include "instance.hpp"

#include "allocator.hpp"
#include <optional>

class Device
{
public:
  static auto create(const Core::Instance& instance,
                     const VkSurfaceKHR& surface) -> Device
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

  auto graphics_queue() const -> VkQueue
  {
    auto queue_result = device.get_queue(vkb::QueueType::graphics);
    if (!queue_result) {
      assert(false && "Failed to get graphics queue");
    }
    return queue_result.value();
  }

  auto get_allocator() -> Allocator& { return allocator; }
  auto get_allocator() const -> const Allocator& { return allocator; }

  auto get_device() const -> VkDevice { return device.device; }
  auto get_physical_device() const -> VkPhysicalDevice
  {
    return device.physical_device.physical_device;
  }
  auto get_timestamp_period() const -> double
  {
    static std::optional<VkPhysicalDeviceProperties> props{ std::nullopt };
    if (!props) {
      props = VkPhysicalDeviceProperties{};
      vkGetPhysicalDeviceProperties(device.physical_device.physical_device,
                                    &props.value());
    }
    return props->limits.timestampPeriod;
  }

  auto graphics_queue_family_index() const -> uint32_t
  {
    auto queue_result = device.get_queue_index(vkb::QueueType::graphics);
    if (!queue_result) {
      assert(false && "Failed to get graphics queue family index");
    }
    return queue_result.value();
  }

private:
  explicit Device(const Core::Instance& instance, const vkb::Device& dev)
    : device(dev)
    , allocator(instance, *this)
  {
  }

  vkb::Device device;
  Allocator allocator;
};