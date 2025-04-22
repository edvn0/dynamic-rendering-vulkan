#pragma once

#include <VkBootstrap.h>
#include <iostream>

namespace Core {

static auto
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
               VkDebugUtilsMessageTypeFlagsEXT,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *) -> VkBool32 {
  std::cerr << "Validation layer: " << callback_data->pMessage << "\n";
  return VK_FALSE;
}

class Instance {
public:
  static auto create() {
    vkb::InstanceBuilder builder;
    auto result = builder.set_app_name("Example Vulkan Application")
                      .request_validation_layers()
                      .require_api_version(1, 3)
                      .use_default_debug_messenger()
                      .set_debug_callback(debug_callback)
                      .build();
    if (!result) {
      std::cerr << "Failed to create Vulkan instance: "
                << result.error().message() << "\n";
      assert(false && "Failed to create Vulkan instance");
    }
    return Instance{
        result.value(),
    };
  }

  auto raw() const -> VkInstance { return instance.instance; }
  auto vkb() const -> const auto & { return instance; }

private:
  explicit Instance(const vkb::Instance &inst) : instance(inst) {}

  vkb::Instance instance;
};

} // namespace Core
