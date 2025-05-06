#pragma once

#include <dynamic_rendering/dynamic_rendering.hpp>

class AppLayer : public ILayer
{
public:
  explicit AppLayer(const Device&, BS::priority_thread_pool* = nullptr);

  auto on_destroy() -> void override;
  auto on_event(Event& event) -> bool override;
  auto on_interface() -> void override;
  auto on_update(double ts) -> void override;
  auto on_render(Renderer& renderer) -> void override;
  auto on_resize(std::uint32_t w, std::uint32_t h) -> void override;

private:
  BS::priority_thread_pool* thread_pool{ nullptr };

  void generate_scene();

  float rotation_speed = 90.f;
  glm::vec2 bounds{};

  std::unique_ptr<VertexBuffer> quad_vertex_buffer;
  std::unique_ptr<IndexBuffer> quad_index_buffer;
  std::unique_ptr<VertexBuffer> cube_vertex_buffer;
  std::unique_ptr<IndexBuffer> cube_index_buffer;

  glm::vec3 light_position{ 28.f, 23.f, 1.f };
  // Orange color
  glm::vec4 light_color{ 1.F, 0.5F, 0.F, 1.F };

  std::vector<glm::mat4> transforms;
};
