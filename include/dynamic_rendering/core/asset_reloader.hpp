#pragma once

#include "core/forward.hpp"
#include "core/util.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class AssetReloader
{
public:
  AssetReloader(BlueprintRegistry&, Renderer&);

  auto handle_dirty_files(const string_hash_set&) -> void;

private:
  auto track_shader_dependencies(const PipelineBlueprint&) -> void;
  auto is_shader_file(std::string_view) const -> bool;

  BlueprintRegistry& blueprint_registry;
  Renderer& renderer;
  string_hash_map<std::unordered_set<std::string>> shader_to_pipeline;
  string_hash_map<std::string> filename_to_material;
};
