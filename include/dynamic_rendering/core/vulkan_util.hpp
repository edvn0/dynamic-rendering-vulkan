#pragma once

#include <type_traits>

namespace Util::Vulkan {

constexpr size_t
get_alignment_requirement(size_t)
{
  return 16;
}

constexpr bool
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
