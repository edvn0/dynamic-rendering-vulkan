#include "allocator.hpp"

#include "device.hpp"
#include "instance.hpp"

Allocator::Allocator(const Core::Instance &instance, const Device &device) {
  VmaAllocatorCreateInfo allocator_info{
      .physicalDevice = device.get_physical_device(),
      .device = device.get_device(),
      .instance = instance.raw(),
  };

  vmaCreateAllocator(&allocator_info, &allocator);
}

Allocator::~Allocator() { vmaDestroyAllocator(allocator); }

auto Allocator::get() const -> VmaAllocator { return allocator; }
