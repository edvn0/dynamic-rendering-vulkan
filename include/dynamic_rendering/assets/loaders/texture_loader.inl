#pragma once

#include <dynamic_rendering/assets/loader.hpp>
#include <dynamic_rendering/core/image.hpp>
#include <dynamic_rendering/renderer/mesh.hpp>

template<>
struct Assets::Loader<Image>
{
  static auto load(const AssetContext& context,
                   const std::filesystem::path& path) -> Assets::Pointer<Image>
  {
    auto image = Assets::make_tracked<Image>();
    if (Image::is_cubemap_externally(path.string())) {
      image = Image::load_cubemap(context.device, path.string());
    } else {
      image = Image::load_from_file(context.device, path.string());
    }

    return image;
  }
};