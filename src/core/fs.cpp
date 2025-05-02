#include "core/fs.hpp"

#include <iostream>

namespace {

std::filesystem::path base_path_value = ".";
std::filesystem::path assets_path_value = base_path_value / "assets";

} // namespace

auto
base_path() -> const std::filesystem::path&
{
  return std::ref(base_path_value);
}

auto
assets_path() -> const std::filesystem::path&
{
  return std::ref(assets_path_value);
}

auto
set_base_path(const std::filesystem::path& path) -> void
{
  if (!std::filesystem::exists(path)) {
    std::cerr << "Path does not exist: " << path.string() << std::endl;
    std::abort();
  }
  if (!std::filesystem::exists(path / "assets")) {
    std::cerr << "Assets folder does not exist in path: " << path.string()
              << std::endl;
    std::abort();
  }
  base_path_value = std::filesystem::canonical(path);
  assets_path_value = base_path_value / "assets";
  std::cout << "Base path set to: " << base_path_value.string() << std::endl;
  std::cout << "Assets path set to: " << assets_path_value.string()
            << std::endl;
}