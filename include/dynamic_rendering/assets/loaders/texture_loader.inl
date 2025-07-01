#pragma once

#include <dynamic_rendering/assets/loader.hpp>
#include <dynamic_rendering/core/image.hpp>
#include <dynamic_rendering/renderer/mesh.hpp>

template<>
struct Assets::Loader<Image>
{
  static auto load(const AssetContext& context,
                   const std::string_view path_view) -> Assets::Pointer<Image>
  {
    const auto path = std::filesystem::path(path_view);
    auto image = Assets::make_tracked<Image>();
    if (Image::is_cubemap_externally(path.string())) {
      image = Image::load_cubemap(context.device, path.string());
    } else {
      image = Image::load_from_file(context.device, path.string(), false);
    }

    return image;
  }
};

template<>
struct Assets::LoaderWithConfig<Image, SampledTextureImageConfiguration>
{
  static auto load(const AssetContext& context,
                   const std::string_view path_view,
                   const SampledTextureImageConfiguration& config)
    -> Assets::Pointer<Image>
  {
    const auto path = std::filesystem::path(path_view);
    auto image = Assets::make_tracked<Image>();
    if (Image::is_cubemap_externally(path.string())) {
      image = Image::load_cubemap(context.device, path.string());
    } else {
      image =
        Image::load_from_file(context.device, path.string(), config, false);
    }

    return image;
  }
};