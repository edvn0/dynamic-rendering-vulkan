#pragma once

#include "core/device.hpp"
#include "core/forward.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image.hpp"
#include "core/util.hpp"
#include "renderer/material.hpp"
#include "vk-maths/aabb.hpp"

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

struct Vertex
{
  glm::vec3 position{ 0.0F }; // offset = 0
  glm::vec3 normal{ 0.0F };   // offset = 12
  glm::vec2 texcoord{ 0.0F }; // offset = 24
  glm::vec4 tangent{ 0.0F };  // offset = 32
};
static_assert(std::is_trivially_copyable_v<Vertex>);
static_assert(sizeof(Vertex) == 48, "Vertex size must be 48 bytes");

struct PositionOnlyVertex
{
  glm::vec3 position{ 0.0F };
};

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
  VkMaths::AABB local_aabb;
};

class StaticMesh
{
public:
  StaticMesh();
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
  [[nodiscard]] auto get_position_only_vertex_buffer() const -> VertexBuffer*
  {
    return position_only_vertex_buffer.get();
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
  [[nodiscard]] auto get_world_aabb(std::size_t submesh_index) const
    -> VkMaths::AABB
  {
    return submeshes.at(submesh_index)
      .local_aabb.transformed(get_world_transform(submesh_index));
  }
  [[nodiscard]] auto get_world_aabb(const Submesh& sm) const -> VkMaths::AABB
  {
    auto& submesh = submeshes.at(submesh_back_pointers.at(&sm));
    return submesh.local_aabb.transformed(get_world_transform(submesh));
  }

  [[nodiscard]] auto get_aabb() const -> const VkMaths::AABB&
  {
    return global_aabb;
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
  std::unique_ptr<VertexBuffer> position_only_vertex_buffer;
  std::unique_ptr<IndexBuffer> index_buffer;

  VkMaths::AABB global_aabb;

  glm::mat4 transform{ 1.0f };

  auto upload_materials(const aiScene*, const Device&, const std::string&)
    -> void;

  friend class MeshCache;

  static inline bool has_initialised_loader{ false };
};
