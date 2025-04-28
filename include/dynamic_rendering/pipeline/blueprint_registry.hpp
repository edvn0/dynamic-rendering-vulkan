#pragma once

#include "core/util.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "pipeline/blueprint_configuration.hpp"

struct PipelineLoadError
{
  std::string message;
};

class BlueprintRegistry
{
public:
  auto load_from_directory(const std::filesystem::path&) -> void;
  auto get(const std::string&) const -> const PipelineBlueprint&;
  [[nodiscard]] auto update(const std::filesystem::path&)
    -> std::expected<void, PipelineLoadError>;
  auto get_all() const -> const auto& { return blueprints; }

private:
  std::
    unordered_map<std::string, PipelineBlueprint, string_hash, std::equal_to<>>
      blueprints;

  auto load_one(const std::filesystem::path&, PipelineBlueprint&) const -> bool;
};
