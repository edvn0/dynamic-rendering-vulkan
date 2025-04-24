#pragma once

#include "device.hpp"
#include "instance.hpp"
#include "window.hpp"

class GUISystem
{
public:
  GUISystem(const Core::Instance&, const Device&, Window&);
  ~GUISystem();

  auto shutdown() -> void;
  auto begin_frame() const -> void;
  auto end_frame(VkCommandBuffer cmd_buf) const -> void;

private:
  auto init_for_vulkan(const Core::Instance& instance,
                       const Device& device,
                       Window& window) const -> void;

  bool destroyed{ false };
};