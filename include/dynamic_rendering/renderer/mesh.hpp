#pragma once

#pragma once

#include "core/device.hpp"
#include "core/forward.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image.hpp"
#include "core/util.hpp"
#include "renderer/material.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texcoord;
};

struct Submesh
{
  std::uint32_t index_offset;
  std::uint32_t index_count;
  std::uint32_t material_index;

  glm::mat4 child_transform{ 1.0F };

  ///
  std::int32_t parent_index{ -1 };
  std::unordered_set<uint32_t> children{};
};

class Mesh
{
public:
  Mesh() = default;
  ~Mesh();

  auto load_from_file(const Device&,
                      const BlueprintRegistry&,
                      const std::string& path) -> bool;
  static auto load_from_memory(const Device&,
                               const BlueprintRegistry&,
                               std::unique_ptr<VertexBuffer>&& vertex_buffer,
                               std::unique_ptr<IndexBuffer>&& index_buffer);

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

private:
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<Submesh> submeshes;
  std::vector<std::unique_ptr<Material>> materials;
  string_hash_map<std::unique_ptr<Image>> loaded_textures;

  std::unique_ptr<VertexBuffer> vertex_buffer;
  std::unique_ptr<IndexBuffer> index_buffer;

  glm::mat4 transform{ 1.0f };

  friend class MeshCache;
};
