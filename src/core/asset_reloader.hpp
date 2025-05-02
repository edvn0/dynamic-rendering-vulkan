#pragma once

#include "core/forward.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class AssetReloader
{
public:
  AssetReloader(BlueprintRegistry& registry, Renderer& renderer);

  auto handle_dirty_files(const std::unordered_set<std::string>& dirty_files);

private:
  void track_shader_dependencies(const PipelineBlueprint& blueprint);
  bool is_shader_file(const std::string& filename) const;

  BlueprintRegistry& blueprint_registry;
  Renderer& renderer;
  std::unordered_map<std::string, std::unordered_set<std::string>>
    shader_to_pipeline;
};
