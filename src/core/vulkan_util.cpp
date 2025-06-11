#include "core/vulkan_util.hpp"

namespace {
PFN_vkCmdBeginDebugUtilsLabelEXT pfn_vkCmdBeginDebugUtilsLabelEXT = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT pfn_vkCmdEndDebugUtilsLabelEXT = nullptr;
bool debug_utils_available = false;
}

namespace Util::Vulkan {

auto
initialise_debug_label(VkDevice device) -> void
{
  if (!debug_utils_available) {
    pfn_vkCmdBeginDebugUtilsLabelEXT =
      reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
    pfn_vkCmdEndDebugUtilsLabelEXT =
      reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
    debug_utils_available = (pfn_vkCmdBeginDebugUtilsLabelEXT != nullptr &&
                             pfn_vkCmdEndDebugUtilsLabelEXT != nullptr);
  }
}

void
cmd_begin_debug_label(VkCommandBuffer cmd,
                      const VkDebugUtilsLabelEXT& label_info)
{
  if (debug_utils_available && pfn_vkCmdBeginDebugUtilsLabelEXT) {
    pfn_vkCmdBeginDebugUtilsLabelEXT(cmd, &label_info);
  }
}
void
cmd_end_debug_label(VkCommandBuffer cmd)
{
  if (debug_utils_available && pfn_vkCmdEndDebugUtilsLabelEXT) {
    pfn_vkCmdEndDebugUtilsLabelEXT(cmd);
  }
}

}