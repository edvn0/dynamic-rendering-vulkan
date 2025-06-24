#pragma once

#include <BS_thread_pool.hpp>
#include <dynamic_rendering/renderer/mesh_cache.hpp>
#include <dynamic_rendering/renderer/renderer.hpp>

#include <dynamic_rendering/assets/asset_allocator.hpp>
#include <dynamic_rendering/assets/handle.hpp>
#include <dynamic_rendering/assets/loader.hpp>

#include <memory>
#include <spdlog/details/registry.h>
#include <string_view>
#include <unordered_map>

namespace Assets {

constexpr std::uint32_t builtin_mesh_count =
  static_cast<std::uint32_t>(MeshType::MaxCount);
constexpr std::uint32_t builtin_texture_count = 2; // White, Black
constexpr std::uint32_t builtin_max_id =
  builtin_mesh_count + builtin_texture_count;
constexpr auto
builtin_cube()
{
  return Handle<StaticMesh>{ static_cast<std::uint32_t>(MeshType::Cube) };
}
constexpr auto
builtin_quad()
{
  return Handle<StaticMesh>{ static_cast<std::uint32_t>(MeshType::Quad) };
}
constexpr auto
builtin_sphere()
{
  return Handle<StaticMesh>{ static_cast<std::uint32_t>(MeshType::Sphere) };
}
constexpr auto
builtin_white_texture()
{
  return Handle<Image>{ builtin_mesh_count + 0 };
}
constexpr auto
builtin_black_texture()
{
  return Handle<Image>{ builtin_mesh_count + 1 };
}

class Manager
{
public:
  template<typename T>
  auto load(std::string_view path) -> Handle<T>;

  template<typename T>
  auto get(Handle<T> handle) -> T*
  {
    return get_impl<>(handle);
  }

  template<typename T>
  auto get(Handle<T> handle) const -> const T*
  {
    return get_impl<>(handle);
  }

  template<typename T>
  auto register_asset(uint32_t id, Assets::Pointer<T> asset) -> void;

  template<typename... Ts>
  auto clear_all() -> void
  {
    (clear<Ts>(), ...);
  }

  static auto initialise(const Device&, BS::priority_thread_pool*) -> void;
  static auto the() -> Manager&;

private:
  Manager(const Device& device, BS::priority_thread_pool* pool)
    : device(device)
    , thread_pool(pool)
  {
  }

  const Device& device;
  BS::priority_thread_pool* thread_pool;
  static inline Manager* singleton;

  template<typename T>
  static auto get_next_id()
  {
    static std::uint32_t next_id = builtin_max_id;
    return next_id++;
  }

  template<typename T>
  using Allocator =
    detail::TrackingAllocator<std::pair<const std::uint32_t, T>>;

  template<typename T>
  using StorageMap = std::unordered_map<std::uint32_t,
                                        Assets::Pointer<T>,
                                        std::hash<std::uint32_t>,
                                        std::equal_to<>,
                                        Allocator<Assets::Pointer<T>>>;

  template<typename T>
  [[nodiscard]] auto storage() const -> StorageMap<T>&
  {
    static_assert(
      std::is_same_v<typename StorageMap<T>::mapped_type, Assets::Pointer<T>>,
      "storage<T> must always use the correct Pointer<T> alias.");
    static StorageMap<T> map;
    static bool first_call{ true };
    if (first_call) {
      first_call = false;
      Logger::log_info("Instantiating storage for: {}", typeid(T).name());
    }
    return map;
  }

  template<typename T>
  auto clear()
  {
    auto& data = storage<T>();
    data.clear();
  }

  template<typename T>
  auto get_impl(Handle<T>) const -> const T*;

  template<typename T>
  auto get_impl(Handle<T>) -> T*;
};

template<typename T>
auto
Manager::get_impl(Handle<T> handle) -> T*
{
  auto& map = storage<T>();
  const auto it = map.find(handle.id);
  return (it != map.end()) ? it->second.get() : nullptr;
}

template<typename T>
auto
Manager::get_impl(Handle<T> handle) const -> const T*
{
  auto& map = storage<T>();
  const auto it = map.find(handle.id);
  return (it != map.end()) ? it->second.get() : nullptr;
}

template<>
inline auto
Manager::get(Handle<StaticMesh> handle) -> StaticMesh*
{
  if (handle.id < builtin_mesh_count)
    return MeshCache::the()
      .get_mesh(static_cast<MeshType>(handle.id))
      .value_or(nullptr);

  return get_impl<StaticMesh>(handle);
}

template<>
inline auto
Manager::get(Handle<StaticMesh> handle) const -> const StaticMesh*
{
  if (handle.id < builtin_mesh_count)
    return MeshCache::the()
      .get_mesh(static_cast<MeshType>(handle.id))
      .value_or(nullptr);

  return get_impl<StaticMesh>(handle);
}

template<>
inline auto
Manager::get(Handle<Material> handle) -> Material*
{
  return get_impl<Material>(handle);
}

template<>
inline auto
Manager::get(Handle<Material> handle) const -> const Material*
{
  return get_impl<Material>(handle);
}

template<>
inline auto
Manager::get(Handle<Image> handle) -> Image*
{
  if (handle.id < builtin_max_id) {
    switch (handle.id) {
      case builtin_white_texture().id:
        return Renderer::get_white_texture();
      case builtin_black_texture().id:
        return Renderer::get_black_texture();
      default:
        return nullptr;
    }
  }

  return get_impl<Image>(handle);
}

template<>
inline auto
Manager::get(Handle<Image> handle) const -> const Image*
{
  if (handle.id < builtin_max_id) {
    switch (handle.id) {
      case builtin_white_texture().id:
        return Renderer::get_white_texture();
      case builtin_black_texture().id:
        return Renderer::get_black_texture();
      default:
        return nullptr;
    }
  }

  return get_impl<Image>(handle);
}

template<typename T>
auto
Manager::register_asset(uint32_t id, Assets::Pointer<T> asset) -> void
{
  auto& data = storage<T>();
  data[id] = std::move(asset);
}

template<typename T>
auto
Manager::load(const std::string_view path) -> Handle<T>
{
  auto id = get_next_id<T>();

  auto asset = Loader<T>::load({ device, thread_pool }, path);
  if (!asset)
    return Handle<T>{}; // invalid handle

  register_asset<T>(id, std::move(asset));
  return Handle<T>{ id };
}
}

#include <dynamic_rendering/assets/handle.inl>
#include <dynamic_rendering/assets/loaders/material_loader.inl>
#include <dynamic_rendering/assets/loaders/static_mesh_loader.inl>
#include <dynamic_rendering/assets/loaders/texture_loader.inl>
