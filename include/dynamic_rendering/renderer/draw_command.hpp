#pragma once

#include "assets/handle.hpp"
#include "core/forward.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct DrawCommand
{
  StaticMesh* mesh{ nullptr };
  Assets::Handle<Material> override_material{};
  std::int32_t submesh_index{ -1 };

  bool operator==(const DrawCommand& rhs) const = default;
};

/// Used for client code, DTO for DrawCommand
struct RendererSubmit
{
  StaticMesh* mesh{ nullptr };
  Assets::Handle<Material> override_material{};
  bool casts_shadows{ false };
  std::uint32_t identifier{ 0 };
};

struct DrawCommandHasher
{
  auto operator()(const DrawCommand& dc) const -> std::size_t;
};

// Represents one instance's transformation
struct InstanceData
{
  glm::mat4 transform;
};

// A single flattened draw entry: which draw command, at what offset in the
// instance buffer, and how many instances
struct DrawItem
{
  DrawCommand command;
  std::uint32_t first_instance{};
  std::uint32_t instance_count{};
};

// Used to submit visible instances after culling
struct DrawInstanceSubmit
{
  const DrawCommand* cmd;
  InstanceData data;
};

using DrawCommandMap =
  std::unordered_map<DrawCommand, std::vector<InstanceData>, DrawCommandHasher>;
using IdentifierMap = std::
  unordered_map<DrawCommand, std::vector<std::uint32_t>, DrawCommandHasher>;
using DrawList = std::vector<DrawItem>;
using DrawListView = std::span<const DrawItem>;