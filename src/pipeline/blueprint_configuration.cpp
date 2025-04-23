#include "blueprint_configuration.hpp"

#include <filesystem>

auto
PipelineBlueprint::hash() const -> std::size_t
{
  std::size_t h = std::hash<std::string>{}(name);
  auto hash_combine = [&](auto&& v) {
    h ^= std::hash<std::decay_t<decltype(v)>>{}(v) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
  };

  hash_combine(static_cast<std::uint32_t>(cull_mode));
  hash_combine(static_cast<std::uint32_t>(polygon_mode));
  hash_combine(blend_enable);
  hash_combine(depth_test);
  hash_combine(depth_write);

  for (const auto& b : bindings) {
    hash_combine(b.binding);
    hash_combine(b.stride);
    hash_combine(static_cast<std::uint32_t>(b.input_rate));
  }

  for (const auto& a : attributes) {
    hash_combine(a.location);
    hash_combine(a.binding);
    hash_combine(static_cast<std::uint32_t>(a.format));
    hash_combine(a.offset);
  }

  for (const auto& s : shader_stages) {
    hash_combine(s.stage);
    hash_combine(
      std::filesystem::last_write_time(s.filepath).time_since_epoch().count());
  }

  return h;
}