#include "doctest/doctest.h"
#include "vk-cvar/cvar_manager.hpp" // Include your CVarManager class

#include <atomic>
#include <barrier>

namespace VkCVar {

TEST_CASE("CVarManager basic functionality")
{
  auto& manager = CVarManager::the();

  manager.set("debug_mode", true);
  manager.set("max_connections", 100);

  CHECK(manager.get<bool>("debug_mode").value() == true);
  CHECK(manager.get<int>("max_connections").value() == 100);
}

TEST_CASE("CVarManager invalid type")
{
  auto& manager = CVarManager::the();
  manager.set("debug_mode", true);

  CHECK_FALSE(manager.get<int>("debug_mode").has_value());
}

struct CVarManagerFixture
{
  CVarManagerFixture() { CVarManager::the().reset(); }

  ~CVarManagerFixture() {}
};

TEST_CASE_FIXTURE(CVarManagerFixture, "CVarManager basic functionality")
{
  auto& manager = CVarManager::the();

  manager.set("debug_mode", true);
  manager.set("max_connections", 100);

  CHECK(manager.get<bool>("debug_mode").value() == true);
  CHECK(manager.get<int>("max_connections").value() == 100);
}

TEST_CASE_FIXTURE(CVarManagerFixture, "CVarManager invalid type")
{
  auto& manager = CVarManager::the();
  manager.set("debug_mode", true);

  CHECK_FALSE(manager.get<int>("debug_mode").has_value());
}

}
