#include "assets/manager.hpp"

#include "core/device.hpp"
#include "pipeline/blueprint_registry.hpp"

namespace Assets {

auto
Manager::initialise(const Device& device, BS::priority_thread_pool* pool)
  -> void
{
  assert(singleton == nullptr);
  singleton = new Manager(device, pool);
}

auto
Manager::the() -> Manager&
{
  assert(singleton != nullptr);
  return *singleton;
}

}
