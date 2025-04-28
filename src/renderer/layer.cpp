#include "renderer/layer.hpp"

#include "core/event_system.hpp"
#include "core/gpu_buffer.hpp"
#include "renderer/renderer.hpp"

#include <array>
#include <execution>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <numeric>
#include <span>

#include <imgui.h>

auto
generate_cube_counter_clockwise(const Device& device)
  -> std::tuple<std::unique_ptr<GPUBuffer>, std::unique_ptr<IndexBuffer>>
{
  struct Vertex
  {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
  };

  static constexpr std::array<Vertex, 24> vertices = { {
    { { -1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
    { { 1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
    { { 1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
    { { -1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },
    { { 1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f }, { 0.f, 0.f } },
    { { -1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 0.f } },
    { { -1.f, 1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f } },
    { { 1.f, 1.f, -1.f }, { 0.f, 0.f, -1.f }, { 0.f, 1.f } },
    { { -1.f, -1.f, -1.f }, { -1.f, 0.f, 0.f }, { 0.f, 0.f } },
    { { -1.f, -1.f, 1.f }, { -1.f, 0.f, 0.f }, { 1.f, 0.f } },
    { { -1.f, 1.f, 1.f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f } },
    { { -1.f, 1.f, -1.f }, { -1.f, 0.f, 0.f }, { 0.f, 1.f } },
    { { 1.f, -1.f, 1.f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f } },
    { { 1.f, -1.f, -1.f }, { 1.f, 0.f, 0.f }, { 1.f, 0.f } },
    { { 1.f, 1.f, -1.f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f } },
    { { 1.f, 1.f, 1.f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f } },
    { { -1.f, 1.f, 1.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f } },
    { { 1.f, 1.f, 1.f }, { 0.f, 1.f, 0.f }, { 1.f, 0.f } },
    { { 1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f } },
    { { -1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f } },
    { { -1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f }, { 0.f, 0.f } },
    { { 1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f }, { 1.f, 0.f } },
    { { 1.f, -1.f, 1.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f } },
    { { -1.f, -1.f, 1.f }, { 0.f, -1.f, 0.f }, { 0.f, 1.f } },
  } };

  static constexpr std::array<std::uint32_t, 36> indices = {
    0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
    12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
  };

  auto vertex_buffer = std::make_unique<GPUBuffer>(
    device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true);
  vertex_buffer->upload(std::span(vertices.data(), vertices.size()));

  auto index_buffer =
    std::make_unique<IndexBuffer>(device, VK_INDEX_TYPE_UINT32);
  index_buffer->upload(std::span(indices.data(), indices.size()));

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

  auto&& [cube_vertex, cube_index] = generate_cube_counter_clockwise(dev);
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

  transforms.reserve(16);
  generate_scene();
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
  auto& light_env = renderer.get_light_environment();
  light_env.light_position = light_position;
  light_env.light_color = light_color;

  if (transforms.empty())
    return;

  // First transform = ground
  renderer.submit(
    {
      .vertex_buffer = quad_vertex_buffer.get(),
      .index_buffer = quad_index_buffer.get(),
      .casts_shadows = true,
    },
    transforms.at(0));

  // All others = cubes
  for (std::size_t i = 1; i < transforms.size(); ++i) {
    renderer.submit(
      {
        .vertex_buffer = cube_vertex_buffer.get(),
        .index_buffer = cube_index_buffer.get(),
        .casts_shadows = true,
      },
      transforms.at(i));
  }

  renderer.submit_lines(
    {
      .vertex_buffer = axes_vertex_buffer.get(),
      .vertex_count = 6,
    },
    glm::scale(glm::mat4{ 1.0F }, glm::vec3{ 5.0F }));
}

auto
Layer::on_resize(std::uint32_t w, std::uint32_t h) -> void
{
  bounds.x = static_cast<float>(w);
  bounds.y = static_cast<float>(h);
}

auto
Layer::generate_scene() -> void
{
  transforms.clear();

  // Ground plane
  transforms.emplace_back(glm::mat4(1.f)); // Identity, no transform

  // Some cubes
  transforms.emplace_back(glm::translate(glm::mat4(1.f), { -3.f, 1.f, -3.f }));
  transforms.emplace_back(glm::translate(glm::mat4(1.f), { 3.f, 1.f, -3.f }));
  transforms.emplace_back(glm::translate(glm::mat4(1.f), { 3.f, 1.f, 3.f }));
  transforms.emplace_back(glm::translate(glm::mat4(1.f), { -3.f, 1.f, 3.f }));

  transforms.emplace_back(glm::translate(glm::mat4(1.f), { 0.f, 5.f, 0.f }) *
                          glm::scale(glm::mat4(1.f), glm::vec3(0.5f)));
}
