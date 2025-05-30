#include "pipeline/blueprint_configuration.hpp"

#include "core/fs.hpp"

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
  hash_combine(static_cast<std::uint32_t>(topology));
  hash_combine(static_cast<std::uint32_t>(winding));
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

    const auto spv_path = assets_path() / "shaders" / (s.filepath + ".spv");
    std::ifstream stream(spv_path, std::ios::binary);
    if (stream) {
      std::vector<char> contents(std::istreambuf_iterator<char>(stream), {});
      std::size_t content_hash = std::hash<std::string_view>{}(
        std::string_view(contents.data(), contents.size()));
      hash_combine(content_hash);
    }
  }

  for (const auto& att : attachments) {
    hash_combine(static_cast<std::uint32_t>(att.format));
    hash_combine(att.blend_enable);
    hash_combine(att.write_mask_rgba);
  }

  if (depth_attachment.has_value()) {
    const auto& d = depth_attachment.value();
    hash_combine(static_cast<std::uint32_t>(d.format));
    hash_combine(d.blend_enable);
    hash_combine(d.write_mask_rgba);
  }

  hash_combine(static_cast<std::uint32_t>(msaa_samples));
  hash_combine(static_cast<std::uint32_t>(depth_compare_op));

  if (depth_bias.has_value()) {
    const auto& d = depth_bias.value();
    hash_combine(d.constant_factor);
    hash_combine(d.clamp);
    hash_combine(d.slope_factor);
  }

  return h;
}