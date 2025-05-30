#include "core/allocator.hpp"

#include "core/device.hpp"
#include "core/instance.hpp"

Allocator::Allocator(const Core::Instance& instance, const Device& device)
{
  VmaAllocatorCreateInfo allocator_info{};
  allocator_info.instance = instance.raw();
  allocator_info.device = device.get_device();
  allocator_info.physicalDevice = device.get_physical_device();

  vmaCreateAllocator(&allocator_info, &allocator);
}

Allocator::~Allocator()
{
  vmaDestroyAllocator(allocator);
}

auto
Allocator::get() const -> VmaAllocator
{
  return allocator;
}
