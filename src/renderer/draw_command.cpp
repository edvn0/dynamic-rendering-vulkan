#include "renderer/draw_command.hpp"

#include "renderer/mesh.hpp"

auto
DrawCommandHasher::operator()(const DrawCommand& dc) const -> std::size_t
{
  const auto h1 = std::hash<StaticMesh*>{}(dc.mesh);
  const auto h2 = std::hash<std::uint32_t>{}(dc.submesh_index);
  const auto h3 = std::hash<std::uint32_t>{}(dc.override_material.id);
  const auto h4 =
    std::hash<float>{}(dc.colour.x) ^ std::hash<float>{}(dc.colour.y) ^
    std::hash<float>{}(dc.colour.z) ^ std::hash<float>{}(dc.colour.w);
  return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
}
