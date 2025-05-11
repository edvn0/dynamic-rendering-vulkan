#include "renderer/mesh.hpp"

#include "core/fs.hpp"
#include "pipeline/blueprint_registry.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "renderer/mesh.hpp"

#include "core/fs.hpp"
#include "pipeline/blueprint_registry.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

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

  auto material_span = std::span(scene->mMaterials, scene->mNumMaterials);

  for (unsigned int i = 0; i < material_span.size(); ++i) {
    aiMaterial* ai_mat = material_span[i];
    string_hash_map<std::tuple<std::uint32_t, std::uint32_t>> binding_info;

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

  std::uint32_t vertex_offset = 0;
  std::uint32_t index_offset = 0;

  for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
    aiMesh* mesh = scene->mMeshes[i];
    for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
      Vertex vertex;
      vertex.position = { mesh->mVertices[v].x,
                          mesh->mVertices[v].y,
                          mesh->mVertices[v].z };
      vertex.normal = mesh->HasNormals() ? glm::vec3{ mesh->mNormals[v].x,
                                                      mesh->mNormals[v].y,
                                                      mesh->mNormals[v].z }
                                         : glm::vec3{ 0.0f };
      vertex.texcoord = mesh->HasTextureCoords(0)
                          ? glm::vec2{ mesh->mTextureCoords[0][v].x,
                                       mesh->mTextureCoords[0][v].y }
                          : glm::vec2{ 0.0f };
      vertices.push_back(vertex);
    }

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
      const aiFace& face = mesh->mFaces[f];
      for (unsigned int j = 0; j < face.mNumIndices; ++j)
        indices.push_back(face.mIndices[j] + vertex_offset);
    }

    submeshes.push_back({
      .index_offset = index_offset,
      .index_count = mesh->mNumFaces * 3,
      .material_index = mesh->mMaterialIndex,
    });

    vertex_offset += mesh->mNumVertices;
    index_offset += mesh->mNumFaces * 3;
  }

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
