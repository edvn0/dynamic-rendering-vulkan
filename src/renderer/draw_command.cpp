#include "renderer/draw_command.hpp"

#include "renderer/mesh.hpp"

auto
DrawCommandHasher::operator()(const DrawCommand& dc) const -> std::size_t
{
  std::size_t h1 = std::hash<Mesh*>{}(dc.mesh);
  std::size_t h2 = std::hash<std::uint32_t>{}(dc.submesh_index);
  std::size_t h3 = std::hash<Material*>{}(dc.override_material);
  std::size_t h4 = std::hash<bool>{}(dc.casts_shadows);
  return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
}
