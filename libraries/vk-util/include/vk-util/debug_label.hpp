#pragma once

#include <span>
#include <string_view>
#include <type_traits>
#include <vulkan.h>

namespace VkUtil {

auto
cmd_begin_debug_label(VkCommandBuffer cmd, const VkDebugUtilsLabelEXT&) -> void;
inline auto
cmd_begin_debug_label(VkCommandBuffer cmd,
                      std::string_view name,
                      const std::span<const float, 4> colour) -> void
{
  VkDebugUtilsLabelEXT label{
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
    .pNext = nullptr,
    .pLabelName = name.data(),
  };
  label.color[0] = colour[0];
  label.color[1] = colour[1];
  label.color[2] = colour[2];
  label.color[3] = colour[3];
  return cmd_begin_debug_label(cmd, label);
}
auto
cmd_end_debug_label(VkCommandBuffer cmd) -> void;
auto initialise_debug_label(VkDevice) -> void;

}