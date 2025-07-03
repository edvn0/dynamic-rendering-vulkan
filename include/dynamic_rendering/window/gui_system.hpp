#pragma once

#include "core/device.hpp"
#include "core/forward.hpp"

#include <vulkan/vulkan.h>

struct ImGuiDescriptorPool
{
  const Device* device{ nullptr };

  std::array<VkDescriptorPoolSize, 1> pool_sizes = {
    { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 } }
  };

  struct PoolData
  {
    VkDescriptorPool pool;
    std::unordered_set<VkDescriptorSet> sets;
  };

  std::vector<PoolData> pools;
  PoolData* current = nullptr;

  explicit ImGuiDescriptorPool(const Device& dev)
    : device(&dev)
  {
    allocate_new_pool();
  }

  ImGuiDescriptorPool() = default;

  ~ImGuiDescriptorPool()
  {
    for (const auto& p : pools) {
      vkDestroyDescriptorPool(device->get_device(), p.pool, nullptr);
    }
  }

  auto allocate_new_pool() -> void
  {
    const VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 10,
      .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
      .pPoolSizes = pool_sizes.data(),
    };

    VkDescriptorPool new_pool;
    if (vkCreateDescriptorPool(
          device->get_device(), &pool_info, nullptr, &new_pool) != VK_SUCCESS) {
      Logger::log_error("Failed to create descriptor pool!");
      std::terminate();
    }

    pools.push_back({ new_pool, {} });
    current = &pools.back();
  }

  [[nodiscard]] auto allocate(VkDescriptorSetLayout layout) -> VkDescriptorSet
  {
    VkDescriptorSetAllocateInfo alloc_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = current->pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout
    };

    VkDescriptorSet set{};
    const auto result =
      vkAllocateDescriptorSets(device->get_device(), &alloc_info, &set);

    if (result == VK_SUCCESS) {
      current->sets.insert(set);
      return set;
    }

    if (result == VK_ERROR_FRAGMENTED_POOL ||
        result == VK_ERROR_OUT_OF_POOL_MEMORY) {
      allocate_new_pool();
      alloc_info.descriptorPool = current->pool;
      if (vkAllocateDescriptorSets(device->get_device(), &alloc_info, &set) !=
          VK_SUCCESS) {
        std::terminate();
      }
      current->sets.insert(set);
      return set;
    }

    std::terminate();
  }

  auto free(const VkDescriptorSet set) -> void
  {
    for (auto& [pool, sets] : pools) {
      if (sets.erase(set)) {
        vkFreeDescriptorSets(device->get_device(), pool, 1, &set);
        return;
      }
    }

    Logger::log_error("Attempted to free unknown descriptor set.");
    std::terminate(); // Or remove if you'd prefer silent failure
  }

  auto reset_all() -> void
  {
    for (auto& [pool, sets] : pools) {
      vkResetDescriptorPool(device->get_device(), pool, 0);
      sets.clear();
    }
  }

  [[nodiscard]] auto get_current_pool() const -> const VkDescriptorPool&
  {
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