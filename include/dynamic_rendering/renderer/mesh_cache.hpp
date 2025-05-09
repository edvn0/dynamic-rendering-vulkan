#pragma once

#include "core/forward.hpp"
#include "core/util.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

enum class MeshType : std::uint8_t
{
  Cube,
  Quad,
  Sphere,
  Cylinder,
  Cone,
  Torus,
};

struct MeshCacheError
{
  std::string message;
};

class MeshCache
{

public:
  ~MeshCache();

  static auto initialise(const Device&, const BlueprintRegistry&) -> void;
  static auto destroy() -> void
  {
    std::lock_guard lock(mutex);

    assert(instance);
    instance.reset();
  }

  static auto the() -> const auto&
  {
    std::lock_guard lock(mutex);
    assert(instance);
    return *instance;
  }

  template<MeshType T>
  auto get_mesh() const -> std::expected<Mesh*, MeshCacheError>
  {
    auto it = meshes.find(T);
    if (it == meshes.end()) {
      return std::unexpected{ MeshCacheError{ "Mesh not found" } };
    }
    if (!it->second) {
      return std::unexpected{ MeshCacheError{ "Mesh not loaded" } };
    }
    return it->second.get();
  }

private:
  explicit MeshCache(const Device&, const BlueprintRegistry&);
  std::unordered_map<MeshType, std::unique_ptr<Mesh>> meshes;
  static inline std::unique_ptr<MeshCache> instance{ nullptr };
  static inline std::mutex mutex{};
};