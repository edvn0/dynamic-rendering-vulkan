#pragma once

#include "core/forward.hpp"

#include <VkBootstrap.h>
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

  auto get_queue_family_index(VkQueue) const -> std::uint32_t;

  auto get_allocator() -> Allocator& { return *allocator; }
  auto get_allocator() const -> const Allocator& { return *allocator; }

  auto get_device() const -> VkDevice { return device.device; }
  auto get_physical_device() const -> VkPhysicalDevice
  {
    return device.physical_device.physical_device;
  }
  auto get_timestamp_period() const -> double;
  auto get_max_sample_count(VkSampleCountFlags = 0) const
    -> VkSampleCountFlagBits;

  auto create_one_time_command_buffer(VkQueue = VK_NULL_HANDLE) const
    -> std::tuple<VkCommandBuffer, VkCommandPool>;
  auto flush(VkCommandBuffer, VkCommandPool, VkQueue = VK_NULL_HANDLE) const
    -> void;
  auto wait_idle() const -> void;

  auto destroy() -> void;

private:
  explicit Device(const Core::Instance&, const vkb::Device&);

  vkb::Device device;
  std::unique_ptr<Allocator> allocator;

  static inline std::optional<VkPhysicalDeviceProperties> props{ std::nullopt };
};