#pragma once

#include <glm/gtc/type_ptr.hpp>
#include <type_traits>

namespace Util::Vulkan {

auto
cmd_begin_debug_label(VkCommandBuffer cmd, const VkDebugUtilsLabelEXT&) -> void;
inline auto
cmd_begin_debug_label(VkCommandBuffer cmd,
                      std::string_view name,
                      const glm::vec4& colour) -> void
{
  VkDebugUtilsLabelEXT label{
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
    .pNext = nullptr,
    .pLabelName = name.data(),
  };
  label.color[0] = colour.r;
  label.color[1] = colour.g;
  label.color[2] = colour.b;
  label.color[3] = colour.a;
  return cmd_begin_debug_label(cmd, label);
}
auto
cmd_end_debug_label(VkCommandBuffer cmd) -> void;
auto initialise_debug_label(VkDevice) -> void;

consteval size_t
get_alignment_requirement(size_t)
{
  return 16;
}

consteval bool
is_properly_aligned(const size_t size)
{
  return (size % 64) == 0;
}

// Macro to check UBO alignment and size

}
#define ASSERT_VULKAN_UBO_COMPATIBLE(type)                                     \
  static_assert(std::is_standard_layout<type>::value,                          \
                #type " must have standard layout");                           \
  static_assert(alignof(type) >=                                               \
                  Util::Vulkan::get_alignment_requirement(sizeof(type)),       \
                #type " alignment must be at least 16 bytes");                 \
  static_assert(Util::Vulkan::is_properly_aligned(sizeof(type)),               \
                #type " size must be a multiple of 64 bytes")
