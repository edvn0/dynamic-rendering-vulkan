#include "renderer/mesh_cache.hpp"

#include "core/device.hpp"
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

MeshCache::MeshCache(const Device& device,
                     const BlueprintRegistry& blueprint_registry)
{

  meshes[MeshType::Cube] = std::make_unique<Mesh>();
  meshes[MeshType::Quad] = std::make_unique<Mesh>();
  meshes[MeshType::Sphere] = std::make_unique<Mesh>();
  meshes[MeshType::Cylinder] = std::make_unique<Mesh>();
  meshes[MeshType::Cone] = std::make_unique<Mesh>();
  meshes[MeshType::Torus] = std::make_unique<Mesh>();

  for (auto&& [type, mesh] : meshes) {
    if (!mesh->load_from_file(
          device, blueprint_registry, "meshes/default/" + to_string(type))) {
      std::cerr << "Failed to load mesh: "
                << "meshes/default/" + to_string(type) << std::endl;
      mesh.reset();
    } else {
      std::cout << "Loaded mesh: " << "meshes/default/" + to_string(type)
                << std::endl;
    }
  }
}