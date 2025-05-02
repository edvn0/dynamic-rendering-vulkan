#include "core/asset_reloader.hpp"

#include "core/fs.hpp"
#include "pipeline/blueprint_configuration.hpp"
#include "pipeline/blueprint_registry.hpp"
#include "renderer/renderer.hpp"

#include <filesystem>
#include <iostream>

AssetReloader::AssetReloader(BlueprintRegistry& registry, Renderer& renderer)
  : blueprint_registry(registry)
  , renderer(renderer)
{
  blueprint_registry.register_callback(
    [this](BlueprintRegistry::BlueprintChangeType,
           const PipelineBlueprint& blueprint) {
      track_shader_dependencies(blueprint);
    });
}

void
AssetReloader::track_shader_dependencies(const PipelineBlueprint& blueprint)
{
  for (const auto& stage : blueprint.shader_stages) {
    if (stage.empty)
      continue;
    const auto shader_path = assets_path() / "shaders" / stage.filepath;
    shader_to_pipeline[shader_path.string()].insert(blueprint.name);
  }
}

void
AssetReloader::handle_dirty_files(const string_hash_set& dirty_files)
{
  using fs = std::filesystem::path;

  for (const auto& file : dirty_files) {
    if (is_shader_file(file)) {
      const auto it = shader_to_pipeline.find(file);
      if (it == shader_to_pipeline.end()) {
        std::cerr << "No pipelines mapped to shader: " << file << std::endl;
        continue;
      }

      for (const auto& pipeline_name : it->second) {
        auto blueprint_path =
          assets_path() / "blueprints" / (pipeline_name + ".yaml");
        if (auto result = blueprint_registry.update(blueprint_path);
            !result.has_value()) {
          std::cerr << "Failed to reload blueprint: " << pipeline_name
                    << std::endl;
        }
      }
      continue;
    }

    const auto filename = fs(file).filename().string();
    auto blueprint_path = assets_path() / "blueprints" / filename;
    if (auto result = blueprint_registry.update(blueprint_path);
        !result.has_value()) {
      std::cerr << "Failed to reload blueprint: " << filename << std::endl;
    }
  }
}

bool
AssetReloader::is_shader_file(const std::string_view filename) const
{
  return filename.ends_with(".spv") || filename.ends_with(".vert") ||
         filename.ends_with(".frag") || filename.ends_with(".glsl");
}
