#include "renderer/mesh_cache.hpp"

#include "core/device.hpp"
#include "pipeline/blueprint_registry.hpp"
#include "renderer/mesh.hpp"

MeshCache::~MeshCache()
{
  std::lock_guard lock(mutex);
  meshes.clear();
}

auto
MeshCache::initialise(const Device& device,
                      const BlueprintRegistry& blueprint_registry) -> void
{
  std::lock_guard lock(mutex);
  assert(!instance);
  instance =
    std::unique_ptr<MeshCache>(new MeshCache(device, blueprint_registry));
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

MeshCache::MeshCache(const Device& dev, const BlueprintRegistry& reg)
  : device(&dev)
  , blueprint_registry(&reg)
{

  meshes[MeshType::Cube] = std::make_unique<Mesh>();
  meshes[MeshType::Quad] = std::make_unique<Mesh>();
  meshes[MeshType::Sphere] = std::make_unique<Mesh>();
  meshes[MeshType::Cylinder] = std::make_unique<Mesh>();
  meshes[MeshType::Cone] = std::make_unique<Mesh>();
  meshes[MeshType::Torus] = std::make_unique<Mesh>();

  /*for (auto&& [type, mesh] : meshes) {
    if (!mesh->load_from_file(
          device, blueprint_registry, "meshes/default/" + to_string(type))) {
      std::cerr << "Failed to load mesh: "
                << "meshes/default/" + to_string(type) << std::endl;
      mesh.reset();
    } else {
      std::cout << "Loaded mesh: " << "meshes/default/" + to_string(type)
                << std::endl;
    }
  }*/

  initialise_cube();
}

auto
generate_cube_counter_clockwise(const Device& device)
{
  struct CubeVertex
  {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
  };
  static constexpr std::array<CubeVertex, 24> vertices = { {
    { { -1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
    { { 1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
    { { 1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
    { { -1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },
    { { 1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f }, { 0.f, 0.f } },
    { { -1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 0.f } },
    { { -1.f, 1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f } },
    { { 1.f, 1.f, -1.f }, { 0.f, 0.f, -1.f }, { 0.f, 1.f } },
    { { -1.f, -1.f, -1.f }, { -1.f, 0.f, 0.f }, { 0.f, 0.f } },
    { { -1.f, -1.f, 1.f }, { -1.f, 0.f, 0.f }, { 1.f, 0.f } },
    { { -1.f, 1.f, 1.f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f } },
    { { -1.f, 1.f, -1.f }, { -1.f, 0.f, 0.f }, { 0.f, 1.f } },
    { { 1.f, -1.f, 1.f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f } },
    { { 1.f, -1.f, -1.f }, { 1.f, 0.f, 0.f }, { 1.f, 0.f } },
    { { 1.f, 1.f, -1.f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f } },
    { { 1.f, 1.f, 1.f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f } },
    { { -1.f, 1.f, 1.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f } },
    { { 1.f, 1.f, 1.f }, { 0.f, 1.f, 0.f }, { 1.f, 0.f } },
    { { 1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f } },
    { { -1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f } },
    { { -1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f }, { 0.f, 0.f } },
    { { 1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f }, { 1.f, 0.f } },
    { { 1.f, -1.f, 1.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f } },
    { { -1.f, -1.f, 1.f }, { 0.f, -1.f, 0.f }, { 0.f, 1.f } },
  } };

  static constexpr std::array<std::uint32_t, 36> indices = {
    0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
    12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
  };

  auto vertex_buffer =
    std::make_unique<VertexBuffer>(device, false, "cube_vertices");
  vertex_buffer->upload_vertices(std::span(vertices));

  auto index_buffer =
    std::make_unique<IndexBuffer>(device, VK_INDEX_TYPE_UINT32, "cube_indices");
  index_buffer->upload_indices(std::span(indices));

  return std::make_pair(std::move(vertex_buffer), std::move(index_buffer));
}

auto
MeshCache::initialise_cube() -> void
{
  auto&& [cube_vertex, cube_index] = generate_cube_counter_clockwise(*device);
  auto& mesh = meshes[MeshType::Cube];
  mesh = std::make_unique<Mesh>();
  mesh->vertex_buffer = std::move(cube_vertex);
  mesh->index_buffer = std::move(cube_index);
  mesh->submeshes.push_back(Submesh{
    .index_offset = 0,
    .index_count = static_cast<std::uint32_t>(mesh->index_buffer->get_count()),
    .material_index = 0,
  });

  mesh->materials.reserve(1);
  mesh->materials.push_back(
    Material::create(*device, blueprint_registry->get("main_geometry"))
      .value());
}