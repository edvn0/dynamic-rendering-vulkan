#pragma once

#include "core/forward.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct DrawCommand
{
  StaticMesh* mesh;
  Material* override_material{ nullptr };
  std::int32_t submesh_index{ -1 };
  bool casts_shadows{ true };

  bool operator==(const DrawCommand& rhs) const = default;
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
  std::uint32_t first_instance;
  std::uint32_t instance_count;
};

// Used to submit visible instances after culling
struct DrawInstanceSubmit
{
  const DrawCommand* cmd;
  InstanceData data;
};

using DrawCommandMap =
  std::unordered_map<DrawCommand, std::vector<InstanceData>, DrawCommandHasher>;
using DrawList = std::vector<DrawItem>;