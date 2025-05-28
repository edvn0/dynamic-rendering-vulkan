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
  enum class BlueprintChangeType : std::uint8_t
  {
    Added,
    Removed,
    Updated,
  };

  using BlueprintChangeCallback =
    std::function<void(BlueprintChangeType, const PipelineBlueprint&)>;

  auto load_from_directory(const std::filesystem::path&) -> void;
  [[nodiscard]] auto get(const std::string&) const -> const PipelineBlueprint&;
  [[nodiscard]] auto update(const std::filesystem::path&)
    -> std::expected<void, PipelineLoadError>;
  [[nodiscard]] auto get_all() const -> const auto& { return blueprints; }
  auto register_callback(BlueprintChangeCallback callback) -> void
  {
    callbacks.push_back(std::move(callback));
  }

private:
  string_hash_map<PipelineBlueprint> blueprints;
  string_hash_map<std::unordered_set<std::string>> shader_to_pipeline;
  static auto load_one(const std::filesystem::path&, PipelineBlueprint&)
    -> bool;

  std::vector<BlueprintChangeCallback> callbacks;
  auto notify_callbacks(const BlueprintChangeType type,
                        const PipelineBlueprint& blueprint) const -> void
  {
    for (const auto& callback : callbacks) {
      callback(type, blueprint);
    }
  }
};
