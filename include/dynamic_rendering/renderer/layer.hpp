#pragma once

#include "core/event_system.hpp"
#include "core/gpu_buffer.hpp"
#include "renderer/renderer.hpp"

#include <glm/glm.hpp>

struct Vertex
{
  glm::vec3 pos{
    0.f,
    0.f,
    0.f,
  };
  glm::vec3 normal{
    0.f,
    0.f,
    1.f,
  };
  glm::vec2 uv{
    0.f,
    0.f,
  };
};

struct ILayer
{
  virtual ~ILayer() = default;
  virtual auto on_destroy() -> void {};
  virtual auto on_event(Event&) -> bool = 0;
  virtual auto on_interface() -> void = 0;
  virtual auto on_update(double) -> void = 0;
  virtual auto on_render(Renderer&) -> void = 0;
  virtual auto on_resize(std::uint32_t, std::uint32_t) -> void = 0;
};

struct Layer final : public ILayer
{
  std::unique_ptr<GPUBuffer> quad_vertex_buffer;
  std::unique_ptr<IndexBuffer> quad_index_buffer;
  std::unique_ptr<GPUBuffer> cube_vertex_buffer;
  std::unique_ptr<IndexBuffer> cube_index_buffer;
  std::unique_ptr<GPUBuffer> axes_vertex_buffer;
  std::vector<glm::mat4> transforms{};
  glm::vec2 bounds{ 0.F };
  float rotation_speed = 3.6f;

  explicit Layer(const Device& dev);

  auto on_destroy() -> void override;
  auto on_event(Event& event) -> bool override;
  auto on_interface() -> void override;
  auto on_update(double ts) -> void override;
  auto on_render(Renderer& renderer) -> void override;
  auto on_resize(std::uint32_t w, std::uint32_t h) -> void override;

private:
  auto generate_scene() -> void;
  glm::vec3 light_position{ 10.f, 20.f, 10.f };
  glm::vec3 light_color{ 1.f, 0.95f, 0.8f };
};