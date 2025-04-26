#include "layer.hpp"
#include "event_system.hpp"
#include "gpu_buffer.hpp"
#include "renderer.hpp"

#include <array>
#include <execution>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <numeric>
#include <span>

#include <imgui.h>

auto
generate_cube(const Device& device)
  -> std::tuple<std::unique_ptr<GPUBuffer>, std::unique_ptr<IndexBuffer>>
{
  std::array<Vertex, 24> vertices = {
    Vertex{ { -0.5f, -0.5f, 0.5f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, 0.5f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, 0.5f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, 0.5f, 0.5f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },

    Vertex{ { 0.5f, -0.5f, -0.5f }, { 0.f, 0.f, -1.f }, { 0.f, 0.f } },
    Vertex{ { -0.5f, -0.5f, -0.5f }, { 0.f, 0.f, -1.f }, { 1.f, 0.f } },
    Vertex{ { -0.5f, 0.5f, -0.5f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f } },
    Vertex{ { 0.5f, 0.5f, -0.5f }, { 0.f, 0.f, -1.f }, { 0.f, 1.f } },

    Vertex{ { -0.5f, -0.5f, -0.5f }, { -1.f, 0.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { -0.5f, -0.5f, 0.5f }, { -1.f, 0.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { -0.5f, 0.5f, 0.5f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, 0.5f, -0.5f }, { -1.f, 0.f, 0.f }, { 0.f, 1.f } },

    Vertex{ { 0.5f, -0.5f, 0.5f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, -0.5f }, { 1.f, 0.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, -0.5f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f } },
    Vertex{ { 0.5f, 0.5f, 0.5f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f } },

    Vertex{ { -0.5f, 0.5f, 0.5f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, 0.5f }, { 0.f, 1.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, -0.5f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, 0.5f, -0.5f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f } },

    Vertex{ { -0.5f, -0.5f, -0.5f }, { 0.f, -1.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, -0.5f }, { 0.f, -1.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, 0.5f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, -0.5f, 0.5f }, { 0.f, -1.f, 0.f }, { 0.f, 1.f } },
  };

  std::array<std::uint32_t, 36> indices = {
    0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
    12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20
  };

  auto vertex_buffer = std::make_unique<GPUBuffer>(
    device,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    true);
  vertex_buffer->upload(std::span(vertices));

  auto index_buffer = std::make_unique<IndexBuffer>(device);
  index_buffer->upload(std::span(indices));

  return { std::move(vertex_buffer), std::move(index_buffer) };
}

Layer::Layer(const Device& dev)
{
  quad_vertex_buffer = std::make_unique<GPUBuffer>(
    dev,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    true);

  std::array<Vertex, 4> square_vertices = {
    Vertex{ { -0.5f, -0.5f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, 0.f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, 0.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, 0.5f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },
  };
  quad_vertex_buffer->upload(std::span(square_vertices));

  quad_index_buffer = std::make_unique<IndexBuffer>(dev);
  std::array<std::uint32_t, 6> square_indices = { 0, 1, 2, 2, 3, 0 };
  quad_index_buffer->upload(std::span(square_indices));

  transforms.resize(100'000);

  auto&& [cube_vertex, cube_index] = generate_cube(dev);
  cube_vertex_buffer = std::move(cube_vertex);
  cube_index_buffer = std::move(cube_index);

  std::array<Vertex, 6> axes_vertices = {
    Vertex{ { 0.f, 0.f, 0.f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { 1.f, 0.f, 0.f }, { 1.f, 0.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { 0.f, 0.f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
    Vertex{ { 0.f, 0.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
  };
  axes_vertex_buffer = std::make_unique<GPUBuffer>(
    dev,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    true);
  axes_vertex_buffer->upload(std::span(axes_vertices));
}

auto
Layer::on_destroy() -> void
{
  quad_vertex_buffer.reset();
  quad_index_buffer.reset();
  cube_vertex_buffer.reset();
  cube_index_buffer.reset();
  axes_vertex_buffer.reset();
}

auto
Layer::on_event(Event& event) -> bool
{
  EventDispatcher dispatch(event);
  dispatch.dispatch<MouseButtonPressedEvent>([](auto&) { return true; });
  dispatch.dispatch<KeyPressedEvent>([](auto&) { return true; });
  dispatch.dispatch<WindowCloseEvent>([](auto&) { return true; });
  return false;
}

auto
Layer::on_interface() -> void
{
  static constexpr auto window = [](const std::string_view name, auto&& fn) {
    ImGui::Begin(name.data());
    fn();
    ImGui::End();
  };

  static ImVec4 clr = { 0.2f, 0.3f, 0.4f, 1.0f };
  window("Controls", [&rs = rotation_speed] {
    ImGui::Text("Adjust settings:");
    ImGui::ColorEdit3("Clear Color", std::bit_cast<float*>(&clr));
    ImGui::SliderFloat("Rotation Speed", &rs, 0.1f, 150.0f);
  });
}

auto
Layer::on_update(double ts) -> void
{
  static float angle = 0.f;
  angle += rotation_speed * static_cast<float>(ts);
  angle = std::fmod(angle, 360.f);

  std::for_each(
    std::execution::par_unseq,
    transforms.begin(),
    transforms.end(),
    [root = transforms.data()](glm::mat4& mat) {
      const std::size_t i = &mat - root;
      const float x = static_cast<float>(i % 10) - 5.f;
      const float y = static_cast<float>(i) / 10.f - 5.f;
      mat = glm::translate(glm::mat4(1.f), { x * 2.f, y * 2.f, 0.f }) *
            glm::rotate(glm::mat4(1.f), glm::radians(angle), { 0.f, 0.f, 1.f });
    });
}

auto
Layer::on_render(Renderer& renderer) -> void
{
  for (const auto& transform : transforms) {
    renderer.submit(
      {
        .vertex_buffer = quad_vertex_buffer.get(),
        .index_buffer = quad_index_buffer.get(),
      },
      transform);
  }

  renderer.submit(
    {
      .vertex_buffer = cube_vertex_buffer.get(),
      .index_buffer = cube_index_buffer.get(),
    },
    transforms.at(0));

  renderer.submit_lines(
    {
      .vertex_buffer = axes_vertex_buffer.get(),
      .vertex_count = 6,
    },
    glm::mat4(1.f));
}

auto
Layer::on_resize(std::uint32_t w, std::uint32_t h) -> void
{
  bounds.x = static_cast<float>(w);
  bounds.y = static_cast<float>(h);
}