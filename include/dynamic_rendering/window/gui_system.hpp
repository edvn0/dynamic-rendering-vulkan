#pragma once

#include "core/forward.hpp"

#include <vulkan/vulkan.h>

class GUISystem
{
public:
  GUISystem(const Core::Instance&, const Device&, Window&, Swapchain&);
  ~GUISystem();

  auto shutdown() -> void;
  auto begin_frame() const -> void;
  auto end_frame(VkCommandBuffer cmd_buf) const -> void;

private:
  auto init_for_vulkan(const Core::Instance&,
                       const Device&,
                       Window&,
                       Swapchain&) const -> void;

  bool destroyed{ false };
};