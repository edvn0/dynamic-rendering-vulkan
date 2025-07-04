#pragma once

#include "core/forward.hpp"
#include "core/util.hpp"
#include "pipeline/blueprint_registry.hpp"

#include <VkBootstrap.h>
#include <optional>

class OneTimeCommand
{
  const Device& device;
  VkQueue chosen_queue{ VK_NULL_HANDLE };
  VkCommandBuffer command_buffer{ VK_NULL_HANDLE };
  VkCommandPool command_pool{ VK_NULL_HANDLE };

public:
  explicit OneTimeCommand(const Device&, VkQueue = VK_NULL_HANDLE);
  ~OneTimeCommand();

  auto operator*() const { return command_buffer; }
};

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
  auto get_physical_device_properties() const
    -> const VkPhysicalDeviceProperties&
  {
    assert(props.has_value() && "Physical device properties not initialized");
    return *props;
  }
  auto get_timestamp_period() const -> double;
  auto get_max_sample_count(VkSampleCountFlags = 0) const
    -> VkSampleCountFlagBits;

  auto create_one_time_command_buffer(VkQueue = VK_NULL_HANDLE) const
    -> std::tuple<VkCommandBuffer, VkCommandPool>;
  auto flush(VkCommandBuffer, VkCommandPool, VkQueue = VK_NULL_HANDLE) const
    -> void;
  auto wait_idle() const -> void;

  auto get_blueprint(const std::string_view name) const
  {
    return blueprint_registry->get(name);
  }
  auto get_all_blueprints() const { return blueprint_registry->get_all(); }
  auto register_blueprint_callback(auto&& func)
  {
    blueprint_registry->register_callback(func);
  };
  auto update_blueprint(const std::filesystem::path& path)
  {
    return blueprint_registry->update(path);
  }

  auto destroy() -> void;
  auto create_resettable_command_pool() const -> VkCommandPool;
  auto allocate_secondary_command_buffer(VkCommandPool) const
    -> VkCommandBuffer;
  auto allocate_primary_command_buffer(VkCommandPool) const -> VkCommandBuffer;

private:
  explicit Device(const Core::Instance&, const vkb::Device&);

  std::unique_ptr<BlueprintRegistry> blueprint_registry;
  vkb::Device device;
  std::unique_ptr<Allocator> allocator;
  VkQueue graphics{};
  VkQueue compute{};
  VkQueue transfer{};
  uint32_t graphics_family{};
  uint32_t compute_family{};
  uint32_t transfer_family{};

  static inline std::optional<VkPhysicalDeviceProperties> props{ std::nullopt };
};