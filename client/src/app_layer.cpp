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

AppLayer::AppLayer(const Device& dev)
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

static constexpr int grid_size_x = 30;
static constexpr int grid_size_y = 20;
static constexpr int grid_size_z = 10;
static constexpr float spacing = 7.f;

auto
AppLayer::on_update(double ts) -> void
{
  ZoneScopedN("On update");

  static glm::vec3 angle_xyz{ 0.f };
  const float delta = rotation_speed * static_cast<float>(ts);
  angle_xyz += glm::vec3{ delta };
  angle_xyz = glm::mod(angle_xyz, glm::vec3{ 360.f });

  static thread_local BS::thread_pool<BS::tp::none> pool{};

  const auto count = transforms.size();
  const std::size_t num_threads = pool.get_thread_count();
  const std::size_t chunk_size = (count + num_threads - 1) / num_threads;

  // We use a latch + detach_task approach here instead of submit_task +
  // future.get(), even though both showed similar performance (~100μs). This
  // avoids per-task allocations and future management overhead, while still
  // providing deterministic synchronization. Since the loop is compute-heavy
  // and bounded, the benefits of using latch outweigh the slight added
  // verbosity. Additionally, this pattern scales better under load and avoids
  // blocking the thread pool on result futures.

  std::latch latch(static_cast<std::ptrdiff_t>(num_threads));

  for (std::size_t t = 0; t < num_threads; ++t) {
    const std::size_t begin = t * chunk_size;
    const std::size_t end = std::min(begin + chunk_size, count);

    if (begin >= end) {
      latch.count_down();
      continue;
    }

    pool.detach_task([=,
                      root = std::span(transforms.data(), transforms.size()),
                      angle = angle_xyz,
                      &latch] {
      ZoneScopedN("Batch rotate");

      for (std::size_t i = begin; i < end; ++i) {
        const auto x = static_cast<std::int32_t>(i % grid_size_x);
        const auto y =
          static_cast<std::int32_t>((i / grid_size_x) % grid_size_y);
        const auto z =
          static_cast<std::int32_t>(i / (grid_size_x * grid_size_y));

        const float offset_x = (x - grid_size_x / 2) * spacing;
        const float offset_y = (y - grid_size_y / 2) * spacing;
        const float offset_z = (z - grid_size_z / 2) * spacing;

        root[i] =
          glm::translate(glm::mat4(1.f), { offset_x, offset_y, offset_z }) *
          glm::rotate(
            glm::mat4(1.f), glm::radians(angle.x), { 0.f, 0.f, 1.f }) *
          glm::rotate(
            glm::mat4(1.f), glm::radians(angle.y), { 0.f, 1.f, 0.f }) *
          glm::rotate(glm::mat4(1.f), glm::radians(angle.z), { 1.f, 0.f, 0.f });
      }

      latch.count_down();
    });
  }

  latch.wait();
}

auto
AppLayer::on_render(Renderer& renderer) -> void
{
  ZoneScopedN("App on_render");

  auto& light_env = renderer.get_light_environment();
  light_env.light_position = light_position;
  light_env.light_color = light_color;

  if (transforms.empty())
    return;

  renderer.submit(
    {
      .vertex_buffer = quad_vertex_buffer.get(),
      .index_buffer = quad_index_buffer.get(),
      .casts_shadows = true,
    },
    transforms.at(0));

  for (std::size_t i = 1; i < transforms.size(); ++i) {
    renderer.submit(
      {
        .vertex_buffer = cube_vertex_buffer.get(),
        .index_buffer = cube_index_buffer.get(),
        .casts_shadows = true,
      },
      transforms.at(i));
  }

  renderer.submit(
    {
      .vertex_buffer = cube_vertex_buffer.get(),
      .index_buffer = cube_index_buffer.get(),
      .override_material = nullptr,
      .casts_shadows = false,
    },
    glm::translate(glm::mat4(1.f), light_position) *
      glm::scale(glm::mat4(1.f), { 0.5f, 0.5f, 0.5f }));

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

  transforms.emplace_back(1.F);

  static constexpr int total = grid_size_x * grid_size_y * grid_size_z;
  transforms.reserve(total + 1);

  for (int z = 0; z < grid_size_z; ++z) {
    for (int y = 0; y < grid_size_y; ++y) {
      for (int x = 0; x < grid_size_x; ++x) {
        const float offset_x = (x - grid_size_x / 2) * spacing;
        const float offset_y = (y - grid_size_y / 2) * spacing;
        const float offset_z = (z - grid_size_z / 2) * spacing;

        glm::mat4 transform =
          glm::translate(glm::mat4(1.f), { offset_x, offset_y, offset_z });
        transforms.emplace_back(transform);
      }
    }
  }
}
