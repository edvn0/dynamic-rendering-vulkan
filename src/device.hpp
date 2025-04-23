#pragma once

#include "instance.hpp"

#include "allocator.hpp"
#include <optional>

class Device
{
public:
  static auto create(const Core::Instance& instance,
                     const VkSurfaceKHR& surface) -> Device;

  auto graphics_queue() const -> VkQueue;
  auto transfer_queue() const -> VkQueue;
  auto compute_queue() const -> VkQueue;
  auto graphics_queue_family_index() const -> uint32_t;
  auto transfer_queue_family_index() const -> uint32_t;
  auto compute_queue_family_index() const -> uint32_t;

  auto get_allocator() -> Allocator& { return allocator; }
  auto get_allocator() const -> const Allocator& { return allocator; }

  auto get_device() const -> VkDevice { return device.device; }
  auto get_physical_device() const -> VkPhysicalDevice
  {
    return device.physical_device.physical_device;
  }
  auto get_timestamp_period() const -> double;

  auto create_one_time_command_buffer() const
    -> std::tuple<VkCommandBuffer, VkCommandPool>;
  auto flush(VkCommandBuffer, VkCommandPool) const -> void;

private:
  explicit Device(const Core::Instance& instance, const vkb::Device& dev)
    : device(dev)
    , allocator(instance, *this)
  {
  }

  vkb::Device device;
  Allocator allocator;
};