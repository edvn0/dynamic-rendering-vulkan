#include "core/debug_utils.hpp"
#include "core/allocator.hpp"
#include "core/device.hpp"

auto
set_debug_name(const Device& device,
               uint64_t handle,
               VkObjectType type,
               std::string_view name) -> void
{
  VkDebugUtilsObjectNameInfoEXT name_info{
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
    .objectType = type,
    .objectHandle = handle,
    .pObjectName = name.data()
  };

  auto func = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
    vkGetDeviceProcAddr(device.get_device(), "vkSetDebugUtilsObjectNameEXT"));

  if (func) {
    func(device.get_device(), &name_info);
  }
}

auto
set_vma_allocation_name(const Device& device,
                        VmaAllocation& allocation,
                        std::string_view name) -> void
{
  vmaSetAllocationName(device.get_allocator().get(), allocation, name.data());
}
