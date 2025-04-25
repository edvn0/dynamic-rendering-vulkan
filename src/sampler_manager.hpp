#pragma once

#include <mutex>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "device.hpp"

namespace detail {

struct sampler_info_hash
{
  auto operator()(const VkSamplerCreateInfo& info) const noexcept -> std::size_t
  {
    std::size_t h = 0;
    h ^= std::hash<int>()(info.magFilter) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>()(info.minFilter) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>()(info.mipmapMode) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>()(info.addressModeU) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>()(info.addressModeV) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>()(info.addressModeW) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

struct sampler_info_equal
{
  auto operator()(const VkSamplerCreateInfo& a,
                  const VkSamplerCreateInfo& b) const noexcept -> bool
  {
    return std::tie(a.magFilter,
                    a.minFilter,
                    a.mipmapMode,
                    a.addressModeU,
                    a.addressModeV,
                    a.addressModeW) == std::tie(b.magFilter,
                                                b.minFilter,
                                                b.mipmapMode,
                                                b.addressModeU,
                                                b.addressModeV,
                                                b.addressModeW);
  }
};

}

class SamplerManager
{
public:
  SamplerManager() = default;

  auto initialize(const Device& device) -> void
  {
    vk_device = device.get_device();
  }

  auto get_sampler(const VkSamplerCreateInfo& create_info) -> const VkSampler&
  {
    std::lock_guard lock(mutex);
    if (cache.contains(create_info))
      return cache.at(create_info);

    VkSampler sampler{};
    VkSamplerCreateInfo info = create_info;
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    vkCreateSampler(vk_device, &info, nullptr, &sampler);
    return cache.try_emplace(info, sampler).first->second;
  }

  auto destroy_all() -> void
  {
    for (auto& [_, sampler] : cache)
      vkDestroySampler(vk_device, sampler, nullptr);
    cache.clear();
  }

private:
  VkDevice vk_device{};
  std::unordered_map<VkSamplerCreateInfo,
                     VkSampler,
                     detail::sampler_info_hash,
                     detail::sampler_info_equal>
    cache;
  std::mutex mutex;
};
