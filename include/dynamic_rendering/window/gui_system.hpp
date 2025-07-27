#pragma once

#include "core/device.hpp"
#include "core/forward.hpp"

#include <vulkan/vulkan.h>

class ImGuiDescriptorPool
{
private:
  const Device* device{ nullptr };
  std::array<VkDescriptorPoolSize, 1> pool_sizes = {
    { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 } }
  };
  auto allocate_new_pool_unlocked() -> void;

  struct PoolData
  {
    VkDescriptorPool pool;
    std::unordered_set<VkDescriptorSet> sets;
  };

  std::vector<PoolData> pools;
  PoolData* current = nullptr;
  mutable std::mutex pool_mutex;

public:
  explicit ImGuiDescriptorPool(const Device& dev)
    : device(&dev)
  {
    allocate_new_pool();
  }
  ImGuiDescriptorPool() = default;
  ~ImGuiDescriptorPool()
  {
    std::lock_guard<std::mutex> lock(pool_mutex);
    for (const auto& p : pools) {
      vkDestroyDescriptorPool(device->get_device(), p.pool, nullptr);
    }
  }

  auto update_set(VkDescriptorSet, const VkDescriptorImageInfo&) -> void;
  auto update_set(VkDescriptorSet, const VkDescriptorBufferInfo&) -> void;
  auto allocate_new_pool() -> void;
  [[nodiscard]] auto allocate(VkDescriptorSetLayout layout) -> VkDescriptorSet;
  auto free(const VkDescriptorSet set) -> void;
  auto reset_all() -> void;

  [[nodiscard]] auto get_current_pool() const -> const VkDescriptorPool&
  {
    std::lock_guard<std::mutex> lock(pool_mutex);
    return current->pool;
  }
};

class GUISystem
{
public:
  GUISystem(const Core::Instance&, const Device&, Window&, Swapchain&);
  ~GUISystem();

  auto shutdown() -> void;
  auto begin_frame() const -> void;
  auto end_frame(VkCommandBuffer cmd_buf) const -> void;

  static auto allocate_image_descriptor_set(VkSampler sampler,
                                            VkImageView view,
                                            VkImageLayout layout)
    -> VkDescriptorSet;
  static auto remove_image_descriptor_set(const VkDescriptorSet&) -> void;

private:
  auto init_for_vulkan(const Core::Instance&,
                       const Device&,
                       Window&,
                       Swapchain&) const -> void;

  bool destroyed{ false };

  static inline std::unique_ptr<ImGuiDescriptorPool> descriptor_pool;
};