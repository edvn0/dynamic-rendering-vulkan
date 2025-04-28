#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

class Device;
namespace Core {
class Instance;
}

class Allocator
{
public:
  explicit Allocator(const Core::Instance&, const Device&);
  ~Allocator();

  Allocator() = delete;
  Allocator(const Allocator&) = delete;
  Allocator(Allocator&&) = delete;
  auto operator=(const Allocator&) -> Allocator& = delete;
  auto operator=(Allocator&&) -> Allocator& = delete;

  auto get() const -> VmaAllocator;

private:
  VmaAllocator allocator{};
};