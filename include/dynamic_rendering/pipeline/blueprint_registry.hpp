#pragma once

#include "core/util.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

#include "pipeline/blueprint_configuration.hpp"

class BlueprintRegistry
{
public:
  auto load_from_directory(const std::filesystem::path&) -> void;
  auto get(const std::string&) const -> const PipelineBlueprint&;

private:
  std::
    unordered_map<std::string, PipelineBlueprint, string_hash, std::equal_to<>>
      blueprints;
};
