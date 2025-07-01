#include "core/image_array.hpp"
#include "assets/asset_allocator.hpp"

ImageArray::~ImageArray()
{
  image.reset();
}

auto
ImageArray::create(const Device& d, const ImageArray::CreateInfo& info)
  -> Assets::Pointer<ImageArray>
{
  auto image = Image::create(
    d,
    {
      .extent = { 1, 1 },
      .format = info.format,
      .array_layers = static_cast<std::uint32_t>(info.layer_extents.size()),
      .usage = info.usage,
      .initial_layout = info.initial_layout,
      .sampler_config = info.sampler_config,
      .allow_in_ui = false,
      .debug_name = info.debug_name,
    });
  return Assets::make_tracked<ImageArray>(std::move(image));
}