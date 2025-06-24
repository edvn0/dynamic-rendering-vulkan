#pragma once

#include "core/device.hpp"
#include "core/forward.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image.hpp"
#include "core/util.hpp"
#include "renderer/material.hpp"

#include <BS_thread_pool.hpp>
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct aiScene;
struct aiMesh;

struct AABB
{
private:
  glm::vec3 minimum;
  glm::vec3 maximum;

public:
  AABB()
    : minimum{ std::numeric_limits<float>::max() }
    , maximum{ std::numeric_limits<float>::lowest() }
  {
  }

  explicit AABB(const glm::vec3& initial_point)
    : minimum{ initial_point }
    , maximum{ initial_point }
  {
  }

  explicit AABB(const glm::vec3& min, const glm::vec3& max)
    : minimum(min)
    , maximum(max)
  {
  }

  [[nodiscard]] auto min() const noexcept -> const glm::vec3&
  {
    return minimum;
  }
  [[nodiscard]] auto max() const noexcept -> const glm::vec3&
  {
    return maximum;
  }

  auto grow(const glm::vec3& point) noexcept
  {
    if (point.x < minimum.x)
      minimum.x = point.x;
    if (point.y < minimum.y)
      minimum.y = point.y;
    if (point.z < minimum.z)
      minimum.z = point.z;

    if (point.x > maximum.x)
      maximum.x = point.x;
    if (point.y > maximum.y)
      maximum.y = point.y;
    if (point.z > maximum.z)
      maximum.z = point.z;
  }

  [[nodiscard]] auto center() const noexcept -> glm::vec3
  {
    return 0.5f * (minimum + maximum);
  }

  [[nodiscard]] auto extent() const noexcept -> glm::vec3
  {
    return maximum - minimum;
  }

  [[nodiscard]] auto transformed(const glm::mat4& m) const noexcept -> AABB
  {
    const std::array<glm::vec3, 8> corners = {
      glm::vec3{ minimum.x, minimum.y, minimum.z },
      { maximum.x, minimum.y, minimum.z },
      { minimum.x, maximum.y, minimum.z },
      { maximum.x, maximum.y, minimum.z },
      { minimum.x, minimum.y, maximum.z },
      { maximum.x, minimum.y, maximum.z },
      { minimum.x, maximum.y, maximum.z },
      { maximum.x, maximum.y, maximum.z }
    };

    AABB result;
    for (const auto& c : corners)
      result.grow(glm::vec3(m * glm::vec4(c, 1.0f)));
    return result;
  }

  [[nodiscard]] auto uniform_scale(const glm::vec3& scaling_factors)
  {
    return AABB(minimum * scaling_factors, maximum * scaling_factors);
  }
};

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texcoord;
  glm::vec4 tangent;
};
static_assert(std::is_trivially_copyable_v<Vertex>);

struct Submesh
{
  std::uint32_t vertex_offset;
  std::uint32_t vertex_count;
  std::uint32_t index_offset;
  std::uint32_t index_count;
  std::uint32_t material_index;

  glm::mat4 child_transform{ 1.0F };

  std::int32_t parent_index{ -1 };
  std::unordered_set<uint32_t> children{};
  AABB local_aabb;
};

class StaticMesh
{
public:
  StaticMesh() = default;
  ~StaticMesh();

  [[nodiscard]] auto load_from_file(const Device&, const std::string& path)
    -> bool;
  [[nodiscard]] auto load_from_file(const Device&,
                                    BS::priority_thread_pool*,
                                    const std::string& path) -> bool;
  [[nodiscard]] auto get_vertex_buffer() const -> VertexBuffer*
  {
    return vertex_buffer.get();
  }
  [[nodiscard]] auto get_index_buffer() const -> IndexBuffer*
  {
    return index_buffer.get();
  }
  [[nodiscard]] auto get_submeshes() const -> const std::vector<Submesh>&
  {
    return submeshes;
  }
  [[nodiscard]] auto get_submesh_index(const Submesh& submesh) const
    -> std::int32_t
  {
    const auto it = std::ranges::find_if(submeshes, [&](const Submesh& s) {
      return s.index_offset == submesh.index_offset &&
             s.index_count == submesh.index_count &&
             s.material_index == submesh.material_index;
    });

    if (it == submeshes.end())
      return -1;
    return static_cast<std::int32_t>(std::distance(submeshes.begin(), it));
  }
  [[nodiscard]] auto get_submesh(const std::uint32_t index) const
    -> const Submesh*
  {
    if (index >= submeshes.size())
      return nullptr;

    return &submeshes.at(index);
  }
  [[nodiscard]] auto get_materials() const -> const auto& { return materials; }
  [[nodiscard]] auto get_material_by_submesh_index(
    const std::int32_t index) const -> Material*
  {
    return materials.at(submeshes.at(index).material_index).get();
  }
  [[nodiscard]] auto get_world_transform(const std::size_t submesh_index) const
    -> glm::mat4
  {
    const auto& submesh = submeshes.at(submesh_index);
    if (submesh.parent_index < 0)
      return submesh.child_transform;

    return get_world_transform(submesh.parent_index) * submesh.child_transform;
  }
  [[nodiscard]] auto get_world_transform(const Submesh& submesh) const
    -> glm::mat4
  {
    if (submesh.parent_index < 0)
      return submesh.child_transform;

    return get_world_transform(submesh.parent_index) * submesh.child_transform;
  }
  [[nodiscard]] auto get_world_aabb(std::size_t submesh_index) const -> AABB
  {
    return submeshes.at(submesh_index)
      .local_aabb.transformed(get_world_transform(submesh_index));
  }
  [[nodiscard]] auto get_world_aabb(const Submesh& sm) const -> AABB
  {
    auto& submesh = submeshes.at(submesh_back_pointers.at(&sm));
    return submesh.local_aabb.transformed(get_world_transform(submesh));
  }

private:
  std::vector<Submesh> submeshes;
  std::unordered_map<const Submesh*, std::uint32_t> submesh_back_pointers;

  auto add_submesh_at_index(std::uint32_t index, const Submesh& submesh)
  {
    submeshes.emplace_back(submesh);
    submesh_back_pointers[&submeshes.back()] = index;
  }

  std::vector<Assets::Pointer<Material>> materials;
  string_hash_map<Assets::Pointer<Image>> loaded_textures;

  std::unique_ptr<VertexBuffer> vertex_buffer;
  std::unique_ptr<IndexBuffer> index_buffer;

  glm::mat4 transform{ 1.0f };

  auto upload_materials(const aiScene*, const Device&, const std::string&)
    -> void;

  friend class MeshCache;
};
