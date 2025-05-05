#pragma once

#include "core/forward.hpp"

#include <string_view>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

auto
set_debug_name(const Device&, std::uint64_t, VkObjectType, std::string_view)
  -> void;
auto
set_vma_allocation_name(const Device&, VmaAllocation&, std::string_view)
  -> void;
