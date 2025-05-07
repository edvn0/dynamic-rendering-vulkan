#pragma once

#include "core/gpu_buffer.hpp"
#include "renderer/material.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct DrawCommand
{
  VertexBuffer* vertex_buffer;
  IndexBuffer* index_buffer;
  Material* override_material{ nullptr };
  bool casts_shadows{ true };

  bool operator==(const DrawCommand& rhs) const = default;
};

struct DrawCommandHasher
{
  auto operator()(const DrawCommand& dc) const -> std::size_t
  {
    std::size_t h1 = std::hash<VertexBuffer*>{}(dc.vertex_buffer);
    std::size_t h2 = std::hash<IndexBuffer*>{}(dc.index_buffer);
    std::size_t h3 = std::hash<Material*>{}(dc.override_material);
    std::size_t h4 = std::hash<bool>{}(dc.casts_shadows);
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
  }
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