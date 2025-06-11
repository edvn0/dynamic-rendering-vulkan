#pragma once

#include <dynamic_rendering/assets/loader.hpp>
#include <dynamic_rendering/renderer/mesh.hpp>

template<>
struct Assets::Loader<Material>
{
  static auto load(const AssetContext& context, const std::string_view name)
    -> Assets::Pointer<Material>
  {
    auto made = make_tracked<Material>();
    if (made->initialise(context.device, name)) {
      return made;
    }

    return nullptr;
  }
};
