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

  auto get_vertex_buffer() const -> VertexBuffer*
  {
    return vertex_buffer.get();
  }
  auto get_index_buffer() const -> IndexBuffer* { return index_buffer.get(); }
  auto get_submeshes() const -> const std::vector<Submesh>&
  {
    return submeshes;
  }
  auto get_submesh_index(const Submesh& submesh) const -> std::int32_t
  {
    constexpr auto same = [](const Submesh& a, const Submesh& b) -> bool {
      return a.index_offset == b.index_offset &&
             a.index_count == b.index_count &&
             a.material_index == b.material_index;
    };

    std::int32_t i = 0;
    for (const auto& sub : submeshes) {
      if (same(sub, submesh)) {
        return i;
      }
      i++;
    }

    return -1;
  }
  auto get_materials() const -> const auto& { return materials; }
  auto get_material_by_submesh_index(const std::int32_t index) const
    -> Material*
  {
    return materials.at(submeshes.at(index).material_index).get();
  }

private:
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<Submesh> submeshes;
  std::vector<std::unique_ptr<Material>> materials;
  string_hash_map<std::unique_ptr<Image>> loaded_textures;

  std::unique_ptr<VertexBuffer> vertex_buffer;
  std::unique_ptr<IndexBuffer> index_buffer;

  friend class MeshCache;
};
