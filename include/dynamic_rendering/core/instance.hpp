#pragma once

#include <VkBootstrap.h>
#include <iostream>

#ifdef IS_DEBUG
static constexpr bool is_debug = true;
#else
static constexpr bool is_debug = false;
#endif

namespace Core {

#ifdef IS_DEBUG
static auto
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
               VkDebugUtilsMessageTypeFlagsEXT,
               const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
               void*) -> VkBool32
{
  Logger::log_error("Validation layer: {}", callback_data->pMessage);
  return VK_FALSE;
}
#endif

class Instance
{
public:
  static auto create()
  {
    vkb::InstanceBuilder builder;
#ifdef IS_DEBUG
    auto result = builder.set_app_name("Example Vulkan Application")
                    .request_validation_layers()
                    .require_api_version(1, 3)
                    .use_default_debug_messenger()
                    .set_debug_callback(debug_callback)
                    .build();
#else
    auto result = builder.set_app_name("Example Vulkan Application")
                    .require_api_version(1, 3)
                    .build();
#endif
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
  auto vkb() const -> const auto& { return instance; }

  auto destroy() -> void { vkb::destroy_instance(instance); }

private:
  explicit Instance(const vkb::Instance& inst)
    : instance(inst)
  {
  }

  vkb::Instance instance;
};

} // namespace Core
