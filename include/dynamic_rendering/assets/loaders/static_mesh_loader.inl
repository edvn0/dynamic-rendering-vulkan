#pragma once

#include <dynamic_rendering/assets/loader.hpp>
#include <dynamic_rendering/renderer/mesh.hpp>

template<>
struct Assets::Loader<StaticMesh>
{
  static auto load(const AssetContext& context,
                   const std::filesystem::path& path)
    -> Assets::Pointer<StaticMesh>
  {
    auto made = make_tracked<StaticMesh>();
    if (!made->load_from_file(
          context.device, context.registry, context.thread_pool, path.string()))
      return nullptr;
    return made;
  }
};
