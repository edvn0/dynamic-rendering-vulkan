#include "app_layer.hpp"

#include <dynamic_rendering/renderer/renderer.hpp>

#include <array>
#include <execution>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <numeric>
#include <span>

#include <imgui.h>
#include <latch>
#include <tracy/Tracy.hpp>

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

auto
generate_cube_counter_clockwise(const Device& device)
{

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

  auto vertex_buffer =
    std::make_unique<VertexBuffer>(device, false, "cube_vertices");
  vertex_buffer->upload_vertices(std::span(vertices));

  auto index_buffer =
    std::make_unique<IndexBuffer>(device, VK_INDEX_TYPE_UINT32, "cube_indices");
  index_buffer->upload_indices(std::span(indices));

  return std::make_pair(std::move(vertex_buffer), std::move(index_buffer));
}

AppLayer::AppLayer(const Device& dev, BS::priority_thread_pool* pool)
  : thread_pool(pool)
{
  quad_vertex_buffer =
    std::make_unique<VertexBuffer>(dev, false, "quad_vertices");

  std::array<Vertex, 4> square_vertices = {
    Vertex{ { -0.5f, -0.5f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, 0.f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, 0.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, 0.5f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },
  };
  quad_vertex_buffer->upload_vertices(std::span(square_vertices));

  quad_index_buffer =
    std::make_unique<IndexBuffer>(dev, VK_INDEX_TYPE_UINT32, "quad_indices");
  std::array<std::uint32_t, 6> square_indices = { 0, 1, 2, 2, 3, 0 };
  quad_index_buffer->upload_indices(std::span(square_indices));

  auto&& [cube_vertex, cube_index] = generate_cube_counter_clockwise(dev);
  cube_vertex_buffer = std::move(cube_vertex);
  cube_index_buffer = std::move(cube_index);

  generate_scene();
}

auto
AppLayer::on_destroy() -> void
{
  quad_vertex_buffer.reset();
  quad_index_buffer.reset();
  cube_vertex_buffer.reset();
  cube_index_buffer.reset();
}

auto
AppLayer::on_event(Event& event) -> bool
{
  EventDispatcher dispatch(event);
  dispatch.dispatch<MouseButtonPressedEvent>([](auto&) { return true; });
  dispatch.dispatch<KeyPressedEvent>([](auto&) { return true; });
  dispatch.dispatch<WindowCloseEvent>([](auto&) { return true; });
  return false;
}

auto
AppLayer::on_interface() -> void
{
  static constexpr auto window = [](const std::string_view name, auto&& fn) {
    if (ImGui::Begin(name.data())) {
      fn();
      ImGui::End();
    }
  };

  window("Controls", [&rs = rotation_speed, this]() {
    ImGui::Text("Adjust settings:");
    ImGui::SliderFloat("Rotation Speed", &rs, 0.1f, 150.0f);
    ImGui::ColorEdit4("Light Color", std::bit_cast<float*>(&light_color));
    ImGui::SliderFloat3(
      "Light Position", std::bit_cast<float*>(&light_position), -100.f, 100.f);
  });
}

auto
AppLayer::on_update(double ts) -> void
{
  ZoneScopedN("On update");

  static float angle_deg = 0.f;
  angle_deg += static_cast<float>(ts) * rotation_speed;
  angle_deg = fmod(angle_deg, 360.f);

  for (std::size_t i = 1; i < transforms.size(); ++i) {
    const auto position = glm::vec3(transforms[i][3]);
    transforms[i] =
      glm::translate(glm::mat4(1.f), position) *
      glm::rotate(glm::mat4(1.f), glm::radians(angle_deg), { 0.f, 1.f, 0.f });
  }
}

auto
AppLayer::on_render(Renderer& renderer) -> void
{
  ZoneScopedN("App on_render");

  auto& light_env = renderer.get_light_environment();
  light_env.light_position = light_position;
  light_env.light_color = light_color;

  if (transforms.size() < 6)
    return;

  // Render ground plane (quad)
  renderer.submit(
    {
      .vertex_buffer = quad_vertex_buffer.get(),
      .index_buffer = quad_index_buffer.get(),
      .casts_shadows = true,
    },
    transforms[0]);

  // Render 5 cubes
  for (std::size_t i = 1; i <= 5; ++i) {
    renderer.submit(
      {
        .vertex_buffer = cube_vertex_buffer.get(),
        .index_buffer = cube_index_buffer.get(),
        .casts_shadows = true,
      },
      transforms[i]);
  }

  // Optional: render light as small cube
  renderer.submit(
    {
      .vertex_buffer = cube_vertex_buffer.get(),
      .index_buffer = cube_index_buffer.get(),
      .override_material = nullptr,
      .casts_shadows = false,
    },
    glm::translate(glm::mat4(1.f), light_position) *
      glm::scale(glm::mat4(1.f), { 0.5f, 0.5f, 0.5f }));

  // Axis lines for debugging
  static constexpr float line_width = 0.1f;
  static constexpr float line_length = 50.f;

  renderer.submit_lines({ 0.f, 0.f, 0.f },
                        { line_length, 0.f, 0.f },
                        line_width,
                        { 1.f, 0.f, 0.f, 1.f });
  renderer.submit_lines({ 0.f, 0.f, 0.f },
                        { 0.f, line_length, 0.f },
                        line_width,
                        { 0.f, 1.f, 0.f, 1.f });
  renderer.submit_lines({ 0.f, 0.f, 0.f },
                        { 0.f, 0.f, line_length },
                        line_width,
                        { 0.f, 0.f, 1.f, 1.f });
}

auto
AppLayer::on_resize(std::uint32_t w, std::uint32_t h) -> void
{
  bounds.x = static_cast<float>(w);
  bounds.y = static_cast<float>(h);
}

auto
AppLayer::generate_scene() -> void
{
  transforms.clear();
  transforms.reserve(6);

  // Ground plane at Y=0, scaled large
  transforms.emplace_back(glm::scale(glm::mat4(1.f), { 200.f, 1.f, 200.f }) *
                          glm::rotate(glm::mat4(1.f),
                                      glm::radians(-90.f),
                                      { 1.f, 0.f, 0.f })); // Rotate to XZ plane

  // Floating cubes at different XZ positions, all at Y = 5.f
  static constexpr float y = 5.f;

  std::array<glm::vec3, 5> positions = {
    glm::vec3{ -20.f, y, -10.f }, glm::vec3{ 10.f, y, -5.f },
    glm::vec3{ -5.f, y, 15.f },   glm::vec3{ 25.f, y, 10.f },
    glm::vec3{ 0.f, y, 0.f },
  };

  for (const auto& pos : positions) {
    transforms.emplace_back(glm::translate(glm::mat4(1.f), pos) *
                            glm::scale(glm::mat4(1.f), { 5.f, 5.f, 5.f }));
  }
}
