#include "renderer/mesh.hpp"

#include "core/fs.hpp"
#include "pipeline/blueprint_registry.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/type_ptr.hpp>
#include <meshoptimizer.h>

namespace {

struct texture_context
{
  aiMaterial* ai_mat;
  const std::string& directory;
  const Device& device;
  string_hash_map<std::unique_ptr<Image>>& cache;
  Material& mat;
};

template<typename Setter>
void
try_upload_texture(texture_context ctx,
                   aiTextureType type,
                   const std::string& slot,
                   Setter&& setter)
{
  aiString tex_path;
  if (ctx.ai_mat->GetTexture(type, 0, &tex_path) != AI_SUCCESS)
    return;

  const std::string full_path = ctx.directory + "/" + tex_path.C_Str();
  Image* image;

  if (auto it = ctx.cache.find(full_path); it != ctx.cache.end()) {
    image = it->second.get();
  } else {
    ctx.cache[full_path] = Image::load_from_file(ctx.device, full_path, false);
    image = ctx.cache[full_path].get();
  }

  ctx.mat.upload(slot, image);
  setter(ctx.mat);
}

} // namespace

Mesh::~Mesh() = default;

auto
Mesh::load_from_file(const Device& device,
                     const BlueprintRegistry& registry,
                     const std::string& path) -> bool
{
  namespace fs = std::filesystem;

  std::vector<fs::path> search_paths = {
    assets_path() / fs::path{ "meshes" } / path,
    assets_path() / fs::path{ "models" } / path
  };

  Assimp::Importer importer;
  const aiScene* scene = nullptr;
  fs::path resolved_path;

  for (const auto& candidate : search_paths) {
    if (fs::exists(candidate)) {
      resolved_path = candidate;
      scene = importer.ReadFile(
        candidate.string(),
        aiProcess_Triangulate | aiProcess_GenSmoothNormals |
          aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices |
          aiProcess_ImproveCacheLocality | aiProcess_RemoveRedundantMaterials |
          aiProcess_SortByPType | aiProcess_FlipUVs);
      if (scene && scene->HasMeshes())
        break;
    }
  }

  if (!scene || !scene->HasMeshes())
    return false;

  const auto directory = resolved_path.parent_path().string();

  for (auto* ai_mat : std::span(scene->mMaterials, scene->mNumMaterials)) {
    auto maybe_mat = Material::create(device, registry.get("main_geometry"));
    if (!maybe_mat.has_value())
      continue;

    auto mat = std::move(maybe_mat.value());
    texture_context ctx{ ai_mat, directory, device, loaded_textures, *mat };

    try_upload_texture(ctx,
                       aiTextureType_DIFFUSE,
                       "albedo_map",
                       [](Material& m) { m.use_albedo_map(); });
    try_upload_texture(ctx,
                       aiTextureType_NORMALS,
                       "normal_map",
                       [](Material& m) { m.use_normal_map(); });
    try_upload_texture(ctx,
                       aiTextureType_METALNESS,
                       "metallic_map",
                       [](Material& m) { m.use_metallic_map(); });
    try_upload_texture(ctx,
                       aiTextureType_DIFFUSE_ROUGHNESS,
                       "roughness_map",
                       [](Material& m) { m.use_roughness_map(); });
    try_upload_texture(ctx,
                       aiTextureType_AMBIENT_OCCLUSION,
                       "ao_map",
                       [](Material& m) { m.use_ao_map(); });
    try_upload_texture(ctx,
                       aiTextureType_EMISSIVE,
                       "emissive_map",
                       [](Material& m) { m.use_emissive_map(); });

    materials.push_back(std::move(mat));
  }

  std::function<void(aiNode*, int)> process_node;
  process_node = [&](aiNode* node, int parent_index) {
    const auto& t = node->mTransformation;
    const auto node_transform = glm::transpose(glm::make_mat4(&t.a1));

    std::vector<std::int32_t> current_node_submesh_indices;

    for (const auto mesh_index : std::span(node->mMeshes, node->mNumMeshes)) {
      const aiMesh* mesh = scene->mMeshes[mesh_index];

      std::vector<Vertex> raw_vertices;
      std::vector<uint32_t> raw_indices;

      auto positions = std::span(mesh->mVertices, mesh->mNumVertices);
      auto normals = mesh->HasNormals()
                       ? std::span(mesh->mNormals, mesh->mNumVertices)
                       : std::span<aiVector3D>{};
      auto texcoords =
        mesh->HasTextureCoords(0)
          ? std::span(mesh->mTextureCoords[0], mesh->mNumVertices)
          : std::span<aiVector3D>{};

      for (std::size_t v = 0; v < positions.size(); ++v) {
        Vertex vertex{};
        vertex.position = { positions[v].x, positions[v].y, positions[v].z };
        vertex.normal =
          normals.empty()
            ? glm::vec3{ 0.0f }
            : glm::vec3{ normals[v].x, normals[v].y, normals[v].z };
        vertex.texcoord = texcoords.empty()
                            ? glm::vec2{ 0.0f }
                            : glm::vec2{ texcoords[v].x, texcoords[v].y };
        raw_vertices.push_back(vertex);
      }

      for (const auto& f : std::span(mesh->mFaces, mesh->mNumFaces)) {
        for (const auto& j : std::span(f.mIndices, f.mNumIndices))
          raw_indices.push_back(j);
      }

      std::vector<uint32_t> remap(raw_indices.size());
      size_t unique_vertex_count =
        meshopt_generateVertexRemap(remap.data(),
                                    raw_indices.data(),
                                    raw_indices.size(),
                                    raw_vertices.data(),
                                    raw_vertices.size(),
                                    sizeof(Vertex));

      std::vector<Vertex> optimized_vertices(unique_vertex_count);
      meshopt_remapVertexBuffer(optimized_vertices.data(),
                                raw_vertices.data(),
                                raw_vertices.size(),
                                sizeof(Vertex),
                                remap.data());

      std::vector<uint32_t> optimized_indices(raw_indices.size());
      meshopt_remapIndexBuffer(optimized_indices.data(),
                               raw_indices.data(),
                               raw_indices.size(),
                               remap.data());

      meshopt_optimizeVertexCache(optimized_indices.data(),
                                  optimized_indices.data(),
                                  optimized_indices.size(),
                                  unique_vertex_count);

      std::vector<float> positions_flat;
      positions_flat.reserve(unique_vertex_count * 3);
      for (const auto& v : optimized_vertices) {
        positions_flat.push_back(v.position.x);
        positions_flat.push_back(v.position.y);
        positions_flat.push_back(v.position.z);
      }

      meshopt_optimizeOverdraw(optimized_indices.data(),
                               optimized_indices.data(),
                               optimized_indices.size(),
                               positions_flat.data(),
                               unique_vertex_count,
                               sizeof(float) * 3,
                               1.05f);

      meshopt_optimizeVertexFetch(optimized_vertices.data(),
                                  optimized_indices.data(),
                                  optimized_indices.size(),
                                  optimized_vertices.data(),
                                  unique_vertex_count,
                                  sizeof(Vertex));
      AABB aabb;
      for (const auto& v : optimized_vertices)
        aabb.grow(v.position);

      const unsigned int cache_size = 32;
      const unsigned int warp_size = 0;
      const unsigned int primgroup_size = 0;

      auto stats_before = meshopt_analyzeVertexCache(raw_indices.data(),
                                                     raw_indices.size(),
                                                     raw_vertices.size(),
                                                     cache_size,
                                                     warp_size,
                                                     primgroup_size);

      auto stats_after = meshopt_analyzeVertexCache(optimized_indices.data(),
                                                    optimized_indices.size(),
                                                    optimized_vertices.size(),
                                                    cache_size,
                                                    warp_size,
                                                    primgroup_size);

      const auto fetch_before = meshopt_analyzeVertexFetch(raw_indices.data(),
                                                           raw_indices.size(),
                                                           raw_vertices.size(),
                                                           sizeof(Vertex))
                                  .bytes_fetched;

      const auto fetch_after =
        meshopt_analyzeVertexFetch(optimized_indices.data(),
                                   optimized_indices.size(),
                                   optimized_vertices.size(),
                                   sizeof(Vertex))
          .bytes_fetched;

      Logger::log_info(
        "Optimized submesh: triangles={}, verts_in={}, verts_out={}, "
        "ACMR={:.2f} → {:.2f}, fetch={} → {} bytes",
        optimized_indices.size() / 3,
        raw_vertices.size(),
        optimized_vertices.size(),
        stats_before.acmr,
        stats_after.acmr,
        fetch_before,
        fetch_after);

      const auto vertex_offset = static_cast<std::uint32_t>(vertices.size());
      const auto index_offset = static_cast<std::uint32_t>(indices.size());

      vertices.insert(
        vertices.end(), optimized_vertices.begin(), optimized_vertices.end());
      indices.insert(
        indices.end(), optimized_indices.begin(), optimized_indices.end());

      const auto submesh_index = static_cast<std::int32_t>(submeshes.size());
      submeshes.push_back(
        { .vertex_offset = vertex_offset,
          .vertex_count = static_cast<uint32_t>(optimized_vertices.size()),
          .index_offset = index_offset,
          .index_count = static_cast<uint32_t>(optimized_indices.size()),
          .material_index = mesh->mMaterialIndex,
          .child_transform = node_transform,
          .parent_index = parent_index,
          .local_aabb = aabb });

      if (parent_index >= 0)
        submeshes[parent_index].children.insert(submesh_index);

      current_node_submesh_indices.push_back(submesh_index);
    }

    const int next_parent = current_node_submesh_indices.empty()
                              ? parent_index
                              : current_node_submesh_indices.front();
    for (const auto& child : std::span(node->mChildren, node->mNumChildren))
      process_node(child, next_parent);
  };

  process_node(scene->mRootNode, -1);

  const auto name = resolved_path.filename().string();
  vertex_buffer =
    std::make_unique<VertexBuffer>(device, false, name + "_vertex_buffer");
  index_buffer = std::make_unique<IndexBuffer>(
    device, VK_INDEX_TYPE_UINT32, name + "_index_buffer");

  vertex_buffer->upload_vertices(std::span(vertices.data(), vertices.size()));
  index_buffer->upload_indices(std::span(indices.data(), indices.size()));

  return true;
}

auto
Mesh::load_from_memory(const Device& device,
                       const BlueprintRegistry& registry,
                       std::unique_ptr<VertexBuffer>&& vertex_buffer,
                       std::unique_ptr<IndexBuffer>&& index_buffer)
{
  auto mesh = std::make_unique<Mesh>();
  mesh->vertex_buffer = std::move(vertex_buffer);
  mesh->index_buffer = std::move(index_buffer);
  mesh->materials.reserve(1);
  mesh->materials.push_back(
    Material::create(device, registry.get("main_geometry")).value());
  return true;
}
