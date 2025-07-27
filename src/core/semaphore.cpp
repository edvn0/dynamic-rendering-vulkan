#include "core/semaphore.hpp"

#include "core/device.hpp"

Semaphore::Semaphore(const Device& dev)
  : device(&dev)
{
  VkSemaphoreCreateInfo semaphore_create_info = {};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = nullptr;

  vkCreateSemaphore(
    device->get_device(), &semaphore_create_info, nullptr, &semaphore);
}

Semaphore::~Semaphore()
{
  vkDestroySemaphore(device->get_device(), semaphore, nullptr);
}