#pragma once

#include <dynamic_rendering/assets/pointer.hpp>
#include <dynamic_rendering/core/forward.hpp>
#include <filesystem>

namespace Assets {

struct AssetContext
{
  const Device& device;
  BS::priority_thread_pool* thread_pool;
};

template<typename T>
struct Loader
{
  static auto load(const AssetContext&, std::string_view)
    -> Assets::Pointer<T> = delete;
};

}