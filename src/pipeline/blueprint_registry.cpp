#include "pipeline/blueprint_registry.hpp"

#include <cassert>
#include <yaml-cpp/yaml.h>

#include "pipeline/yaml_converters.hpp"

auto
BlueprintRegistry::load_from_directory(const std::filesystem::path& path)
  -> void
{
  const auto assets_path = std::filesystem::current_path() / path;
  for (const auto& entry : std::filesystem::directory_iterator(assets_path)) {
    if (entry.is_regular_file() && entry.path().extension() == ".yaml") {

      PipelineBlueprint blueprint;
      if (load_one(entry.path(), blueprint)) {
        blueprints[blueprint.name] = std::move(blueprint);
      } else {
        std::cerr << "Failed to load blueprint: " << entry.path() << std::endl;
      }
    }
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
  if (load_one(path, blueprint)) {
    const auto filename_stem = path.stem().string();
    blueprints[filename_stem] = std::move(blueprint);
    return {};
  } else {
    return std::unexpected<PipelineLoadError>{ { "Failed to load blueprint" } };
  }
}

auto
BlueprintRegistry::load_one(const std::filesystem::path& file_path,
                            PipelineBlueprint& output_blueprint) const -> bool
{
  if (auto node = YAML::LoadFile(file_path.string());
      !YAML::convert<PipelineBlueprint>::decode(node, output_blueprint)) {
    return false;
  }
  output_blueprint.full_path = file_path;
  return true;
}