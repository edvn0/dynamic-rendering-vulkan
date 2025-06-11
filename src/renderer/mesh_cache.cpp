#include "renderer/mesh_cache.hpp"

#include "core/device.hpp"
#include "pipeline/blueprint_registry.hpp"
#include "renderer/mesh.hpp"

MeshCache::~MeshCache()
{
  meshes.clear();
}

auto
MeshCache::initialise(const Device& device) -> void
{
  std::lock_guard lock(mutex);
  assert(!instance);
  instance = std::unique_ptr<MeshCache>(new MeshCache(device));
}

static constexpr auto
to_string(MeshType type) -> std::string
{
  switch (type) {
    case MeshType::Cube:
      return "cube";
    case MeshType::Quad:
      return "quad";
    case MeshType::Sphere:
      return "sphere";
    case MeshType::Cylinder:
      return "cylinder";
    case MeshType::Cone:
      return "cone";
    case MeshType::Torus:
      return "torus";
  }
  return {};
}

MeshCache::MeshCache(const Device& dev)
  : device(&dev)
{

  meshes[MeshType::Cube] = std::make_unique<StaticMesh>();
  meshes[MeshType::Quad] = std::make_unique<StaticMesh>();
  meshes[MeshType::Sphere] = std::make_unique<StaticMesh>();
  meshes[MeshType::Cylinder] = std::make_unique<StaticMesh>();
  meshes[MeshType::Cone] = std::make_unique<StaticMesh>();
  meshes[MeshType::Torus] = std::make_unique<StaticMesh>();

  for (auto&& [type, mesh] : meshes) {
    if (!mesh->load_from_file(*device, "meshes/default/" + to_string(type))) {
      mesh.reset();
    }
  }

  initialise_cube();
}

template<bool OnlyPositions = false>
auto
generate_cube_counter_clockwise(const Device& device)
{
  if constexpr (OnlyPositions) {
    struct Vertex
    {
      glm::vec3 position;
    };

    static constexpr std::array<Vertex, 24> vertices = { {
      { { -1.f, -1.f, 1.f } },  { { 1.f, -1.f, 1.f } },
      { { 1.f, 1.f, 1.f } },    { { -1.f, 1.f, 1.f } },
      { { 1.f, -1.f, -1.f } },  { { -1.f, -1.f, -1.f } },
      { { -1.f, 1.f, -1.f } },  { { 1.f, 1.f, -1.f } },
      { { -1.f, -1.f, -1.f } }, { { -1.f, -1.f, 1.f } },
      { { -1.f, 1.f, 1.f } },   { { -1.f, 1.f, -1.f } },
      { { 1.f, -1.f, 1.f } },   { { 1.f, -1.f, -1.f } },
      { { 1.f, 1.f, -1.f } },   { { 1.f, 1.f, 1.f } },
      { { -1.f, 1.f, 1.f } },   { { 1.f, 1.f, 1.f } },
      { { 1.f, 1.f, -1.f } },   { { -1.f, 1.f, -1.f } },
      { { -1.f, -1.f, -1.f } }, { { 1.f, -1.f, -1.f } },
      { { 1.f, -1.f, 1.f } },   { { -1.f, -1.f, 1.f } },
    } };

    static constexpr std::array<std::uint32_t, 36> indices = {
      0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
      12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
    };

    auto vertex_buffer =
      std::make_unique<VertexBuffer>(device, false, "cube_vertices_pos");
    vertex_buffer->upload_vertices(std::span(vertices));

    auto index_buffer = std::make_unique<IndexBuffer>(
      device, VK_INDEX_TYPE_UINT32, "cube_indices");
    index_buffer->upload_indices(std::span(indices));

    return std::make_pair(std::move(vertex_buffer), std::move(index_buffer));
  } else {
    struct Vertex
    {
      glm::vec3 position;
      glm::vec3 normal;
      glm::vec2 uv;
      glm::vec4 tangent;
    };

    static constexpr std::array<Vertex, 24> vertices = { {
      // Front (+Z), tangent +X
      { { -1.f, -1.f, 1.f },
        { 0.f, 0.f, 1.f },
        { 0.f, 0.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { 1.f, -1.f, 1.f },
        { 0.f, 0.f, 1.f },
        { 1.f, 0.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { 1.f, 1.f, 1.f },
        { 0.f, 0.f, 1.f },
        { 1.f, 1.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { -1.f, 1.f, 1.f },
        { 0.f, 0.f, 1.f },
        { 0.f, 1.f },
        { 1.f, 0.f, 0.f, 1.f } },

      // Back (-Z), tangent -X
      { { 1.f, -1.f, -1.f },
        { 0.f, 0.f, -1.f },
        { 0.f, 0.f },
        { -1.f, 0.f, 0.f, 1.f } },
      { { -1.f, -1.f, -1.f },
        { 0.f, 0.f, -1.f },
        { 1.f, 0.f },
        { -1.f, 0.f, 0.f, 1.f } },
      { { -1.f, 1.f, -1.f },
        { 0.f, 0.f, -1.f },
        { 1.f, 1.f },
        { -1.f, 0.f, 0.f, 1.f } },
      { { 1.f, 1.f, -1.f },
        { 0.f, 0.f, -1.f },
        { 0.f, 1.f },
        { -1.f, 0.f, 0.f, 1.f } },

      // Left (-X), tangent -Z
      { { -1.f, -1.f, -1.f },
        { -1.f, 0.f, 0.f },
        { 0.f, 0.f },
        { 0.f, 0.f, -1.f, 1.f } },
      { { -1.f, -1.f, 1.f },
        { -1.f, 0.f, 0.f },
        { 1.f, 0.f },
        { 0.f, 0.f, -1.f, 1.f } },
      { { -1.f, 1.f, 1.f },
        { -1.f, 0.f, 0.f },
        { 1.f, 1.f },
        { 0.f, 0.f, -1.f, 1.f } },
      { { -1.f, 1.f, -1.f },
        { -1.f, 0.f, 0.f },
        { 0.f, 1.f },
        { 0.f, 0.f, -1.f, 1.f } },

      // Right (+X), tangent +Z
      { { 1.f, -1.f, 1.f },
        { 1.f, 0.f, 0.f },
        { 0.f, 0.f },
        { 0.f, 0.f, 1.f, 1.f } },
      { { 1.f, -1.f, -1.f },
        { 1.f, 0.f, 0.f },
        { 1.f, 0.f },
        { 0.f, 0.f, 1.f, 1.f } },
      { { 1.f, 1.f, -1.f },
        { 1.f, 0.f, 0.f },
        { 1.f, 1.f },
        { 0.f, 0.f, 1.f, 1.f } },
      { { 1.f, 1.f, 1.f },
        { 1.f, 0.f, 0.f },
        { 0.f, 1.f },
        { 0.f, 0.f, 1.f, 1.f } },

      // Top (+Y), tangent +X
      { { -1.f, 1.f, 1.f },
        { 0.f, 1.f, 0.f },
        { 0.f, 0.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { 1.f, 1.f, 1.f },
        { 0.f, 1.f, 0.f },
        { 1.f, 0.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { 1.f, 1.f, -1.f },
        { 0.f, 1.f, 0.f },
        { 1.f, 1.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { -1.f, 1.f, -1.f },
        { 0.f, 1.f, 0.f },
        { 0.f, 1.f },
        { 1.f, 0.f, 0.f, 1.f } },

      // Bottom (-Y), tangent +X
      { { -1.f, -1.f, -1.f },
        { 0.f, -1.f, 0.f },
        { 0.f, 0.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { 1.f, -1.f, -1.f },
        { 0.f, -1.f, 0.f },
        { 1.f, 0.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { 1.f, -1.f, 1.f },
        { 0.f, -1.f, 0.f },
        { 1.f, 1.f },
        { 1.f, 0.f, 0.f, 1.f } },
      { { -1.f, -1.f, 1.f },
        { 0.f, -1.f, 0.f },
        { 0.f, 1.f },
        { 1.f, 0.f, 0.f, 1.f } },
    } };

    static constexpr std::array<std::uint32_t, 36> indices = {
      0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
      12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
    };

    auto vertex_buffer =
      std::make_unique<VertexBuffer>(device, false, "cube_vertices_full");
    vertex_buffer->upload_vertices(std::span(vertices));

    auto index_buffer = std::make_unique<IndexBuffer>(
      device, VK_INDEX_TYPE_UINT32, "cube_indices");
    index_buffer->upload_indices(std::span(indices));

    return std::make_pair(std::move(vertex_buffer), std::move(index_buffer));
  }
}

auto
MeshCache::initialise_cube() -> void
{
  {
    auto&& [cube_vertex, cube_index] = generate_cube_counter_clockwise(*device);
    auto& mesh = meshes[MeshType::Cube];
    mesh = std::make_unique<StaticMesh>();
    mesh->vertex_buffer = std::move(cube_vertex);
    mesh->index_buffer = std::move(cube_index);
    mesh->add_submesh_at_index(0,
                               Submesh{
                                 .index_offset = 0,
                                 .index_count = static_cast<std::uint32_t>(
                                   mesh->index_buffer->get_count()),
                                 .material_index = 0,
                               });

    mesh->materials.reserve(1);
    mesh->materials.push_back(
      Material::create(*device, "main_geometry").value());
  }

  {
    auto&& [cube_vertex, cube_index] =
      generate_cube_counter_clockwise<true>(*device);
    auto& mesh = meshes[MeshType::CubeOnlyPosition];
    mesh = std::make_unique<StaticMesh>();
    mesh->vertex_buffer = std::move(cube_vertex);
    mesh->index_buffer = std::move(cube_index);
    mesh->add_submesh_at_index(0,
                               Submesh{
                                 .index_offset = 0,
                                 .index_count = static_cast<std::uint32_t>(
                                   mesh->index_buffer->get_count()),
                                 .material_index = 0,
                               });

    mesh->materials.reserve(1);
    mesh->materials.push_back(
      Material::create(*device, "main_geometry").value());
  }
}