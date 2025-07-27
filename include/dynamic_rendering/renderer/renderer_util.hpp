#pragma once

#include "mesh.hpp"
#include "renderer/draw_command.hpp"
#include "renderer/material.hpp"

namespace Util::Renderer {

template<typename VertexType>
auto
get_vertex_buffer(const StaticMesh* mesh) -> const auto*
{
  if constexpr (std::is_same_v<VertexType, Vertex>) {
    return mesh->get_vertex_buffer();
  } else if constexpr (std::is_same_v<VertexType, PositionOnlyVertex>) {
    return mesh->get_position_only_vertex_buffer();
  }
}

template<typename VertexType>
auto
bind_mesh_buffers(const VkCommandBuffer& cmd,
                  const DrawCommand& cmd_info,
                  const Submesh* submesh,
                  const GPUBuffer& instance_vertex_buffer) -> void
{
  static_assert(std::is_same_v<VertexType, Vertex> ||
                std::is_same_v<VertexType, PositionOnlyVertex>);

  const auto& vertex_buffer = get_vertex_buffer<VertexType>(cmd_info.mesh);
  const auto& index_buffer = cmd_info.mesh->get_index_buffer();

  const VkDeviceSize vertex_offset_bytes =
    submesh->vertex_offset * sizeof(VertexType);
  const std::array vertex_buffers = { vertex_buffer->get(),
                                      instance_vertex_buffer.get() };
  const std::array offsets = { vertex_offset_bytes, 0ULL };

  vkCmdBindVertexBuffers(cmd,
                         0,
                         static_cast<std::uint32_t>(vertex_buffers.size()),
                         vertex_buffers.data(),
                         offsets.data());
  vkCmdBindIndexBuffer(
    cmd, index_buffer->get(), 0, index_buffer->get_index_type());
}

}