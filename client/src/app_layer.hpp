#pragma once

#include <dynamic_rendering/dynamic_rendering.hpp>

class AppLayer : public ILayer
{
public:
  explicit AppLayer(const Device& device);

  auto on_destroy() -> void override;
  auto on_event(Event& event) -> bool override;
  auto on_interface() -> void override;
  auto on_update(double ts) -> void override;
  auto on_render(Renderer& renderer) -> void override;
  auto on_resize(std::uint32_t w, std::uint32_t h) -> void override;

private:
  void generate_scene();

  float rotation_speed = 90.f;
  glm::vec2 bounds{};

  std::unique_ptr<GPUBuffer> quad_vertex_buffer;
  std::unique_ptr<IndexBuffer> quad_index_buffer;
  std::unique_ptr<GPUBuffer> cube_vertex_buffer;
  std::unique_ptr<IndexBuffer> cube_index_buffer;
  std::unique_ptr<GPUBuffer> axes_vertex_buffer;

  glm::vec3 light_position{ 0.f, 0.f, 1.f };
  glm::vec3 light_color{ 1.f, 1.f, 1.f };

  std::vector<glm::mat4> transforms;
};
