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
  // Seed the shader_to_pipeline map with the current blueprints
  for (const auto& [name, blueprint] : blueprint_registry.get_all()) {
    track_shader_dependencies(blueprint);
  }

  blueprint_registry.register_callback(
    [this](BlueprintRegistry::BlueprintChangeType,
           const PipelineBlueprint& blueprint) {
      track_shader_dependencies(blueprint);
    });
}

void
AssetReloader::track_shader_dependencies(const PipelineBlueprint& blueprint)
{
  filename_to_material[blueprint.full_path.string()] = blueprint.name;
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
  for (const auto& dirty : dirty_files) {
    const auto path = std::filesystem::path(dirty);

    if (path.extension() == ".spv") {
      auto original_shader_path = path;
      original_shader_path.replace_extension(); // strip .spv

      reload_blueprints_for_shader(original_shader_path);
    } else if (path.extension() == ".yaml") {
      reload_blueprint_and_material(path);
    }
  }
}

bool
AssetReloader::is_shader_file(const std::string_view filename) const
{
  return filename.ends_with(".spv") || filename.ends_with(".vert") ||
         filename.ends_with(".frag") || filename.ends_with(".glsl");
}

auto
AssetReloader::reload_blueprint_and_material(
  const std::filesystem::path& blueprint_path) -> void
{
  const auto abs_path = blueprint_path.lexically_normal();

  if (auto result = blueprint_registry.update(abs_path); !result.has_value()) {
    Logger::log_error("Failed to reload blueprint: {}. Error: {}",
                      abs_path.string(),
                      result.error().message);
    return;
  }

  const std::string abs_str = abs_path.string();

  auto it = filename_to_material.find(abs_str);
  if (it == filename_to_material.end()) {
    Logger::log_error("Failed to find material for blueprint: {}", abs_str);
    return;
  }

  const auto& blueprint = blueprint_registry.get(it->second);
  auto* mat = renderer.get_material_by_name(blueprint.name);

  if (!mat) {
    Logger::log_error(
      "Failed to find material for blueprint: {}. Material not found.",
      blueprint.name);
    return;
  }

  mat->reload(blueprint);
  Logger::log_info("Reloaded material: {}", blueprint.name);
}

auto
AssetReloader::reload_blueprints_for_shader(
  const std::filesystem::path& shader_path) -> void
{
  const std::string shader_key = shader_path.string();

  const auto it = shader_to_pipeline.find(shader_key);
  if (it == shader_to_pipeline.end()) {
    Logger::log_error("Failed to find pipeline for shader: {}", shader_key);
    return;
  }

  for (const auto& pipeline_name : it->second) {
    const auto blueprint_path =
      assets_path() / "blueprints" / (pipeline_name + ".yaml");
    reload_blueprint_and_material(blueprint_path);
  }
}
