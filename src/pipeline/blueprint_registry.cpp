#include "pipeline/blueprint_registry.hpp"

#include "core/fs.hpp"
#include "pipeline/yaml_converters.hpp"

#include <cassert>
#include <filesystem>
#include <yaml-cpp/yaml.h>

static constexpr auto yaml_extensions =
  std::array<std::string_view, 2>{ ".yaml", ".yml" };

static constexpr auto is_valid_extension = [](const auto& path) {
  return std::ranges::any_of(yaml_extensions, [&path](const auto& ext) {
    return path.extension() == ext;
  });
};

auto
map_shader_to_pipeline(PipelineBlueprint& blueprint, auto& shader_to_pipeline)
  -> void
{
  for (const auto& stage : blueprint.shader_stages) {
    if (stage.empty) {
      continue;
    }
    const auto shader_path = assets_path() / "shaders" / stage.filepath;
    shader_to_pipeline[shader_path.string()].insert(blueprint.name);
  }
}

auto
BlueprintRegistry::load_from_directory(const std::filesystem::path& path)
  -> void
{

  for (const auto base = assets_path() / path;
       const auto& entry : std::filesystem::directory_iterator(base)) {
    if (!entry.is_regular_file() || !is_valid_extension(entry.path())) {
      continue;
    }

    PipelineBlueprint blueprint;
    if (!load_one(entry.path(), blueprint)) {
      std::cerr << "Failed to load blueprint: " << entry.path() << std::endl;
      continue;
    }

    map_shader_to_pipeline(blueprint, shader_to_pipeline);
    blueprints[blueprint.name] = std::move(blueprint);
  }
}

auto
BlueprintRegistry::get(const std::string& name) const
  -> const PipelineBlueprint&
{
  assert(blueprints.contains(name) &&
         "BlueprintRegistry::get: Blueprint not found");

  return blueprints.at(name);
}

auto
BlueprintRegistry::update(const std::filesystem::path& path)
  -> std::expected<void, PipelineLoadError>
{
  if (!std::filesystem::exists(path)) {
    return std::unexpected<PipelineLoadError>{ { "File does not exist" } };
  }

  PipelineBlueprint blueprint;
  if (!load_one(path, blueprint)) {
    return std::unexpected<PipelineLoadError>{ { "Failed to load blueprint" } };
  }

  const bool is_new = !blueprints.contains(blueprint.name);
  const auto name = blueprint.name;
  blueprints[blueprint.name] = std::move(blueprint);

  map_shader_to_pipeline(blueprints.at(name), shader_to_pipeline);
  notify_callbacks(is_new ? BlueprintChangeType::Added
                          : BlueprintChangeType::Updated,
                   blueprints.at(name));
  return {};
}

auto
BlueprintRegistry::load_one(const std::filesystem::path& file_path,
                            PipelineBlueprint& output_blueprint) -> bool
{
  std::cout << "Loading blueprint " << file_path.string() << std::endl;
  if (const auto node = YAML::LoadFile(file_path.string());
      !YAML::convert<PipelineBlueprint>::decode(node, output_blueprint)) {
    return false;
  }

  output_blueprint.full_path = file_path;
  output_blueprint.name = file_path.stem().string();
  std::cout << "Done loading blueprint\n";
  return true;
}