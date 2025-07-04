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
  AssetReloader(Device&, Renderer&);

  auto handle_dirty_files(const string_hash_set&) -> void;

private:
  auto track_shader_dependencies(const PipelineBlueprint&) -> void;
  auto is_shader_file(std::string_view) const -> bool;

  Device* device;
  Renderer* renderer;
  string_hash_map<std::unordered_set<std::string>> shader_to_pipeline;
  string_hash_map<std::string> filename_to_material;

  auto reload_blueprint_and_material(const std::filesystem::path&) -> void;
  auto reload_blueprints_for_shader(const std::filesystem::path&) -> void;
};
