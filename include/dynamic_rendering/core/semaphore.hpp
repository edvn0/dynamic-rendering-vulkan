#pragma once

#include "core/config.hpp"
#include "core/forward.hpp"

#include <vulkan/vulkan.h>

class Semaphore
{
public:
  explicit Semaphore(const Device&);
  Semaphore() = default;
  ~Semaphore();

  [[nodiscard]] auto get_semaphore() const -> const auto& { return semaphore; }

private:
  const Device* device{ nullptr };
  VkSemaphore semaphore{ VK_NULL_HANDLE };
};

class SemaphoreArray
{
public:
  explicit SemaphoreArray(const Device& device)
  {
    for (auto& s : semaphores) {
      s = std::make_unique<Semaphore>(device);
    }
  }
  SemaphoreArray() = default;
  ~SemaphoreArray() { clear(); }

  [[nodiscard]] auto at(const std::uint32_t i) const -> const auto&
  {
    return semaphores.at(i)->get_semaphore();
  }

  [[nodiscard]] auto get_semaphore(const std::uint32_t i) const -> const auto&
  {
    return at(i);
  }

  auto clear() -> void
  {
    for (auto& s : semaphores) {
      s.reset();
    }
  }

private:
  frame_array<std::unique_ptr<Semaphore>> semaphores;
};