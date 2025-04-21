
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "VkBootstrap.h"
#include <GLFW/glfw3.h>

auto init_vulkan(auto *surface) {
  vkb::InstanceBuilder builder;
  auto inst_ret = builder.set_app_name("Example Vulkan Application")
                      .request_validation_layers()
                      .use_default_debug_messenger()
                      .build();

  if (!inst_ret) { /* report */
  }
  vkb::Instance vkb_inst = inst_ret.value();

  vkb::PhysicalDeviceSelector selector{vkb_inst};

  auto phys_ret = selector.set_surface(surface)
                      .set_minimum_version(1, 1)
                      .require_dedicated_transfer_queue()
                      .select();

  if (!phys_ret) { /* report */
  }

  vkb::DeviceBuilder device_builder{phys_ret.value()};

  auto dev_ret = device_builder.build();

  if (!dev_ret) { /* report */
  }

  vkb::Device vkb_device = dev_ret.value();

  auto graphics_queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);

  if (!graphics_queue_ret) { /* report */
  }

  VkQueue graphics_queue = graphics_queue_ret.value();
}

struct Window {
  GLFWwindow *window;
  VkSurfaceKHR surface;
};

auto main(int argc, char **argv) -> std::int32_t {
  std::cout << "Hello world!\n";
  init_vulkan();

  return 0;
}
