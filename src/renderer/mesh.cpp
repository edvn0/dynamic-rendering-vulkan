#include "renderer/mesh.hpp"

#include "core/fs.hpp"
#include "pipeline/blueprint_registry.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/type_ptr.hpp>
#include <meshoptimizer.h>
#include <tracy/Tracy.hpp>

namespace {

struct SubmeshPerformanceStats
{
  size_t total_submeshes{ 0 };
  size_t total_triangles{ 0 };
  size_t total_vertices_in{ 0 };
  size_t total_vertices_out{ 0 };
  float total_acmr_before{ 0.f };
  float total_acmr_after{ 0.f };
  size_t total_fetch_before{ 0 };
  size_t total_fetch_after{ 0 };

  unsigned int cache_size{ 32 };
  unsigned int warp_size{ 0 };
  unsigned int primgroup_size{ 0 };

  void accumulate(const std::vector<Vertex>& raw_vertices,
                  const std::vector<Vertex>& optimized_vertices,
                  const std::vector<uint32_t>& raw_indices,
                  const std::vector<uint32_t>& optimized_indices)
  {
    static std::mutex mutex;
    std::unique_lock lock(mutex);

    ++total_submeshes;
    total_triangles += optimized_indices.size() / 3;
    total_vertices_in += raw_vertices.size();
    total_vertices_out += optimized_vertices.size();

    const auto stats_before = meshopt_analyzeVertexCache(raw_indices.data(),
                                                         raw_indices.size(),
                                                         raw_vertices.size(),
                                                         cache_size,
                                                         warp_size,
                                                         primgroup_size);

    const auto stats_after =
      meshopt_analyzeVertexCache(optimized_indices.data(),
                                 optimized_indices.size(),
                                 optimized_vertices.size(),
                                 cache_size,
                                 warp_size,
                                 primgroup_size);

    total_acmr_before += stats_before.acmr;
    total_acmr_after += stats_after.acmr;

    const auto fetch_before = meshopt_analyzeVertexFetch(raw_indices.data(),
                                                         raw_indices.size(),
                                                         raw_vertices.size(),
                                                         sizeof(Vertex));
    const auto fetch_after =
      meshopt_analyzeVertexFetch(optimized_indices.data(),
                                 optimized_indices.size(),
                                 optimized_vertices.size(),
                                 sizeof(Vertex));

    total_fetch_before += fetch_before.bytes_fetched;
    total_fetch_after += fetch_after.bytes_fetched;
  }

  void log_summary() const
  {
    Logger::log_info("Submesh optimization summary:");
    Logger::log_info("  Total: {} submeshes", total_submeshes);
    Logger::log_info("  Triangles: {}", total_triangles);
    Logger::log_info(
      "  Vertices (in - out): {} → {}", total_vertices_in, total_vertices_out);
    Logger::log_info("  ACMR avg: {:.2f} to {:.2f}",
                     total_acmr_before / total_submeshes,
                     total_acmr_after / total_submeshes);
    Logger::log_info(
      "  Fetch total: {} to {} bytes", total_fetch_before, total_fetch_after);
  }
};

struct texture_context
{
  aiMaterial* ai_mat;
  const std::string& directory;
  const Device& device;
  string_hash_map<Assets::Pointer<Image>>& cache;
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

  static std::mutex cache_mutex;
  {
    std::scoped_lock lock(cache_mutex);
    if (auto it = ctx.cache.find(full_path); it != ctx.cache.end()) {
      image = it->second.get();
    } else {
      ctx.cache[full_path] =
        Image::load_from_file(ctx.device, full_path, false);
      image = ctx.cache[full_path].get();
    }
  }

  ctx.mat.upload(slot, image);
  setter(ctx.mat);
}

LoadedSubmesh
process_mesh_impl(const aiMesh* mesh,
                  const glm::mat4& transform,
                  const int parent_index,
                  SubmeshPerformanceStats& stats)
{
  std::vector<Vertex> raw_vertices(mesh->mNumVertices);
  for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
    raw_vertices[i].position = { mesh->mVertices[i].x,
                                 mesh->mVertices[i].y,
                                 mesh->mVertices[i].z };
    raw_vertices[i].normal = mesh->HasNormals()
                               ? glm::vec3{ mesh->mNormals[i].x,
                                            mesh->mNormals[i].y,
                                            mesh->mNormals[i].z }
                               : glm::vec3{ 0.0f };
    raw_vertices[i].texcoord = mesh->HasTextureCoords(0)
                                 ? glm::vec2{ mesh->mTextureCoords[0][i].x,
                                              mesh->mTextureCoords[0][i].y }
                                 : glm::vec2{ 0.0f };
    if (mesh->HasTangentsAndBitangents()) {
      const glm::vec3 tangent = glm::normalize(glm::vec3{
        mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z });
      const glm::vec3 bitangent =
        glm::normalize(glm::vec3{ mesh->mBitangents[i].x,
                                  mesh->mBitangents[i].y,
                                  mesh->mBitangents[i].z });
      const glm::vec3 normal = glm::normalize(raw_vertices[i].normal);
      const float handedness =
        (glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f) ? -1.0f
                                                                  : 1.0f;
      raw_vertices[i].tangent = glm::vec4(tangent, handedness);
    } else {
      raw_vertices[i].tangent = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
  }

  std::vector<uint32_t> raw_indices;
  for (const auto& face : std::span(mesh->mFaces, mesh->mNumFaces))
    raw_indices.insert(
      raw_indices.end(), face.mIndices, face.mIndices + face.mNumIndices);

  std::vector<uint32_t> remap(raw_indices.size());
  const size_t unique_vertex_count =
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

  stats.accumulate(
    raw_vertices, optimized_vertices, raw_indices, optimized_indices);

  return LoadedSubmesh{ std::move(optimized_vertices),
                        std::move(optimized_indices),
                        aabb,
                        mesh->mMaterialIndex,
                        transform,
                        parent_index };
}

void
upload_materials_impl_secondary(
  const aiScene* scene,
  const Device& device,
  const std::string& directory,
  string_hash_map<Assets::Pointer<Image>>& loaded_textures,
  std::vector<Assets::Pointer<Material>>& materials)
{
  struct ThreadResult
  {
    VkCommandPool pool{ nullptr };
    VkCommandBuffer cmd{ nullptr };
    Assets::Pointer<Material> mat{ nullptr };
    std::vector<std::unique_ptr<GPUBuffer>>
      staging_buffers; // Changed to vector
  };

  static std::mutex cache_mutex;

  std::vector<std::future<ThreadResult>> futures;
  std::vector<ThreadResult> completed;

  for (auto* ai_mat : std::span(scene->mMaterials, scene->mNumMaterials)) {
    futures.emplace_back(
      std::async(std::launch::async, [&, ai_mat]() -> ThreadResult {
        ThreadResult result{};

        auto maybe_mat = Material::create(device, "main_geometry");
        if (!maybe_mat.has_value())
          return result;

        result.mat = std::move(maybe_mat.value());
        result.pool = device.create_resettable_command_pool();
        result.cmd = device.allocate_secondary_command_buffer(result.pool);

        VkCommandBufferInheritanceInfo inheritance{};
        inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = &inheritance;

        if (vkBeginCommandBuffer(result.cmd, &begin_info) != VK_SUCCESS)
          return {};

        auto thread_safe_try_upload = [&](const aiTextureType type,
                                          const std::string& slot,
                                          auto&& setter) {
          aiString tex_path;
          if (ai_mat->GetTexture(type, 0, &tex_path) != AI_SUCCESS)
            return;

          const std::string full_path = directory + "/" + tex_path.C_Str();
          Image* image = nullptr;
          bool needs_staging = false;

          {
            std::scoped_lock lock(cache_mutex);
            if (const auto it = loaded_textures.find(full_path);
                it != loaded_textures.end()) {
              image = it->second.get();
            } else {
              // Load image with staging buffer
              auto image_result = Image::load_from_file_with_staging(
                device, full_path, false, true, result.cmd);
              if (!image_result.image)
                return;

              image = image_result.image.get();
              // Store staging buffer in our result to keep it alive
              if (image_result.staging) {
                result.staging_buffers.push_back(
                  std::move(image_result.staging));
              }

              // Cache the loaded image
              loaded_textures[full_path] = std::move(image_result.image);
              needs_staging = true;
            }
          }

          // Upload to material
          if (image != nullptr) {
            result.mat->upload(slot, image);
            setter(*result.mat);
          }
        };

        thread_safe_try_upload(aiTextureType_DIFFUSE,
                               "albedo_map",
                               [](Material& m) { m.use_albedo_map(); });
        thread_safe_try_upload(aiTextureType_NORMALS,
                               "normal_map",
                               [](Material& m) { m.use_normal_map(); });
        thread_safe_try_upload(aiTextureType_METALNESS,
                               "metallic_map",
                               [](Material& m) { m.use_metallic_map(); });
        thread_safe_try_upload(aiTextureType_DIFFUSE_ROUGHNESS,
                               "roughness_map",
                               [](Material& m) { m.use_roughness_map(); });
        thread_safe_try_upload(aiTextureType_AMBIENT_OCCLUSION,
                               "ao_map",
                               [](Material& m) { m.use_ao_map(); });
        thread_safe_try_upload(aiTextureType_EMISSIVE,
                               "emissive_map",
                               [](Material& m) { m.use_emissive_map(); });

        if (vkEndCommandBuffer(result.cmd) != VK_SUCCESS)
          return {};

        return result;
      }));
  }

  // Collect all completed results
  for (auto& f : futures) {
    if (auto result = f.get(); result.cmd != VK_NULL_HANDLE)
      completed.push_back(std::move(result));
  }

  // Execute all secondary command buffers if we have any
  if (!completed.empty()) {
    const auto vk_device = device.get_device();
    const auto queue = device.graphics_queue();

    const auto primary_pool = device.create_resettable_command_pool();
    auto primary_cmd = device.allocate_primary_command_buffer(primary_pool);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(primary_cmd, &begin_info);

    std::vector<VkCommandBuffer> secondary_cmds;
    secondary_cmds.reserve(completed.size());
    for (const auto& r : completed)
      secondary_cmds.push_back(r.cmd);

    vkCmdExecuteCommands(primary_cmd,
                         static_cast<uint32_t>(secondary_cmds.size()),
                         secondary_cmds.data());

    vkEndCommandBuffer(primary_cmd);

    constexpr VkFenceCreateInfo fence_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    VkFence fence;
    vkCreateFence(vk_device, &fence_info, nullptr, &fence);

    const VkSubmitInfo submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &primary_cmd,
    };

    vkQueueSubmit(queue, 1, &submit_info, fence);
    vkWaitForFences(vk_device, 1, &fence, VK_TRUE, UINT64_MAX);

    // Clean up synchronization objects
    vkDestroyFence(vk_device, fence, nullptr);
    vkDestroyCommandPool(vk_device, primary_pool, nullptr);
  }

  // Now it's safe to move materials and clean up resources
  // The staging buffers will be destroyed here after GPU work is complete
  for (auto& r : completed) {
    if (r.mat)
      materials.push_back(std::move(r.mat));
    vkDestroyCommandPool(device.get_device(), r.pool, nullptr);
    // staging_buffers vector will automatically clean up when r goes out of
    // scope
  }
}

} // namespace

StaticMesh::~StaticMesh() = default;

auto
StaticMesh::load_from_file(const Device& device, const std::string& path)
  -> bool
{
  ZoneScopedN("Load from file (ST)");

  namespace fs = std::filesystem;

  std::vector search_paths = {
    fs::path{ assets_path() / fs::path{ "meshes" } / path }.make_preferred(),
    fs::path{ assets_path() / fs::path{ "models" } / path }.make_preferred(),
  };

  Assimp::Importer importer;
  const aiScene* scene = nullptr;
  fs::path resolved_path;

  for (const auto& candidate : search_paths) {
    if (!fs::exists(candidate)) {
      continue;
    }
    if (!fs::is_regular_file(candidate)) {
      continue;
    }
    resolved_path = candidate;
    scene = importer.ReadFile(
      candidate.string(),
      aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality | aiProcess_RemoveRedundantMaterials |
        aiProcess_SortByPType | aiProcess_FlipUVs);
    if (!scene || !scene->HasMeshes()) {
      auto err = importer.GetErrorString();
      Logger::log_error("Failed to load model: {}. Error: {}",
                        resolved_path.generic_string(),
                        err);
    }
    if (scene && scene->HasMeshes())
      break;
  }

  if (!scene) {
    Logger::log_error("Failed to load model: {}. No valid scene found.", path);
    return false;
  }

  const auto directory = resolved_path.parent_path().string();

  for (auto* ai_mat : std::span(scene->mMaterials, scene->mNumMaterials)) {
    auto maybe_mat = Material::create(device, "main_geometry");
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

  for (auto i = 0; i < submeshes.size(); ++i) {
    submesh_back_pointers[&submeshes.at(i)] = i;
  }

  return true;
}

auto
StaticMesh::load_from_file(const Device& device,
                           BS::priority_thread_pool* thread_pool,
                           const std::string& path) -> bool
{
  ZoneScopedN("Load from file (MT)");

  namespace fs = std::filesystem;

  std::vector<fs::path> search_paths = {
    fs::path{ assets_path() / fs::path{ "meshes" } / path }.make_preferred(),
    fs::path{ assets_path() / fs::path{ "models" } / path }.make_preferred(),
  };

  Assimp::Importer importer;
  const aiScene* scene = nullptr;
  fs::path resolved_path;

  for (const auto& candidate : search_paths) {
    if (!fs::exists(candidate))
      continue;
    if (!fs::is_regular_file(candidate))
      continue;
    resolved_path = candidate;
    scene = importer.ReadFile(
      candidate.string(),
      aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality | aiProcess_RemoveRedundantMaterials |
        aiProcess_SortByPType | aiProcess_FlipUVs);
    if (scene && scene->HasMeshes())
      break;
    Logger::log_error("Failed to load model: {}. Error: {}",
                      resolved_path.generic_string(),
                      importer.GetErrorString());
  }

  if (!scene) {
    Logger::log_error("Failed to load model: {}. No valid scene found.", path);
    return false;
  }

  const auto directory = resolved_path.parent_path().string();
  upload_materials(scene, device, directory);

  std::vector<std::future<LoadedSubmesh>> futures;
  std::vector<int> future_to_parent_mapping;

  SubmeshPerformanceStats perf_stats;

  std::function<void(aiNode*, int)> process_node;
  process_node = [&](aiNode* node, int parent_index) {
    const glm::mat4 transform =
      glm::transpose(glm::make_mat4(&node->mTransformation.a1));

    std::vector<int> current_node_future_indices;

    // Process all meshes for this node
    for (const auto mesh_index : std::span(node->mMeshes, node->mNumMeshes)) {
      const aiMesh* mesh = scene->mMeshes[mesh_index];

      int future_index = static_cast<int>(futures.size());
      current_node_future_indices.push_back(future_index);
      future_to_parent_mapping.push_back(parent_index);

      futures.push_back(thread_pool->submit_task([=, &perf_stats] {
        return process_mesh_impl(mesh, transform, parent_index, perf_stats);
      }));
    }

    // Determine the parent index for child nodes
    // Use the first future index of this node as the parent for children
    const int next_parent = current_node_future_indices.empty()
                              ? parent_index
                              : current_node_future_indices.front();

    // Process child nodes with the correct parent
    for (const auto& child : std::span(node->mChildren, node->mNumChildren))
      process_node(child, next_parent);
  };

  process_node(scene->mRootNode, -1);

  // Process futures in order to maintain correct indexing
  std::vector<LoadedSubmesh> results;
  results.reserve(futures.size());

  for (auto& future : futures) {
    results.push_back(future.get());
  }

  std::unordered_map<int, std::vector<std::int32_t>> future_parent_to_children;

  for (size_t i = 0; i < results.size(); ++i) {
    const auto vertex_offset = static_cast<std::uint32_t>(vertices.size());
    const auto index_offset = static_cast<std::uint32_t>(indices.size());

    auto& result = results[i];
    vertices.insert(
      vertices.end(), result.vertices.begin(), result.vertices.end());
    indices.insert(indices.end(), result.indices.begin(), result.indices.end());

    const auto submesh_index = static_cast<std::int32_t>(submeshes.size());
    const auto vertex_count =
      static_cast<std::uint32_t>(result.vertices.size());
    const auto index_count = static_cast<std::uint32_t>(result.indices.size());

    // Map future parent index to actual submesh index
    int actual_parent_index = -1;
    if (future_to_parent_mapping[i] >= 0) {
      actual_parent_index = future_to_parent_mapping[i];
    }

    submeshes.push_back(Submesh{
      .vertex_offset = vertex_offset,
      .vertex_count = vertex_count,
      .index_offset = index_offset,
      .index_count = index_count,
      .material_index = result.material_index,
      .child_transform = result.transform,
      .parent_index = actual_parent_index,
      .children = {},
      .local_aabb = result.aabb,
    });

    if (actual_parent_index >= 0) {
      future_parent_to_children[actual_parent_index].push_back(submesh_index);
    }
  }

  // Apply parent-child relationships
  for (auto& [future_parent, children] : future_parent_to_children) {
    if (future_parent < static_cast<int>(submeshes.size())) {
      for (int32_t child : children) {
        submeshes[future_parent].children.insert(child);
      }
    }
  }

  const auto name = resolved_path.filename().string();
  vertex_buffer =
    std::make_unique<VertexBuffer>(device, false, name + "_vertex_buffer");
  index_buffer = std::make_unique<IndexBuffer>(
    device, VK_INDEX_TYPE_UINT32, name + "_index_buffer");
  vertex_buffer->upload_vertices(std::span(vertices.data(), vertices.size()));
  index_buffer->upload_indices(std::span(indices.data(), indices.size()));

  perf_stats.log_summary();

  for (auto i = 0; i < submeshes.size(); ++i) {
    submesh_back_pointers[&submeshes.at(i)] = i;
  }

  return true;
}

auto
StaticMesh::upload_materials(const aiScene* scene,
                             const Device& device,
                             const std::string& directory) -> void
{
  upload_materials_impl_secondary(
    scene, device, directory, loaded_textures, materials);
}
