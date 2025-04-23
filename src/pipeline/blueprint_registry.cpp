#include "blueprint_registry.hpp"

#include <cassert>
#include <yaml-cpp/yaml.h>

#include "yaml_converters.hpp"

auto
BlueprintRegistry::load_from_directory(const std::filesystem::path& path)
  -> void
{
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
      YAML::Node yaml = YAML::LoadFile(entry.path().string());
      PipelineBlueprint blueprint = yaml.as<PipelineBlueprint>();
      blueprints[blueprint.name] = std::move(blueprint);
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
