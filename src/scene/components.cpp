#include "scene/components.hpp"

#include "assets/handle.hpp"
#include "assets/manager.hpp"
#include "core/logger.hpp"
#include "renderer/mesh.hpp"
#include "renderer/mesh_cache.hpp"

namespace Component {

Mesh::Mesh(const std::string_view path)
{
  if (const auto asset = Assets::Manager::the().load<StaticMesh>(path);
      asset.is_valid()) {
    mesh = asset;
  } else {
    Logger::log_debug("Could not load static mesh at path: {}", path);
  }
}

Mesh::Mesh(const Assets::Handle<StaticMesh> m)
  : mesh(m)
{
}

}