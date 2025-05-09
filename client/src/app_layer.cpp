#include "app_layer.hpp"

#include "dynamic_rendering/renderer/mesh.hpp"

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

struct CubeVertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

auto
generate_cube_counter_clockwise(const Device& device)
{
  static constexpr std::array<CubeVertex, 24> vertices = { {
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

AppLayer::AppLayer(const Device& dev,
                   BS::priority_thread_pool* pool,
                   BlueprintRegistry* registry)
  : thread_pool(pool)
  , blueprint_registry(registry)
{
  auto&& [cube_vertex, cube_index] = generate_cube_counter_clockwise(dev);
  cube_vertex_buffer = std::move(cube_vertex);
  cube_index_buffer = std::move(cube_index);
  mesh = std::make_unique<Mesh>();

  if (mesh->load_from_file(dev, *blueprint_registry, "cerberus/scene.gltf")) {
  }
  generate_scene();
}

AppLayer::~AppLayer() = default;

auto
AppLayer::on_destroy() -> void
{
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

  renderer.submit(
    {
      .mesh = mesh.get(),
    },
    glm::mat4(1.f));

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

  auto maybe_cube_mesh = MeshCache::the().get_mesh<MeshType::Cube>();

  if (!maybe_cube_mesh.has_value()) {
    return;
  }

  auto cube_mesh = maybe_cube_mesh.value();

  for (const auto& transform : transforms) {
    renderer.submit(
      {
        .mesh = cube_mesh,
        .casts_shadows = true,
      },
      transform);
  }
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

  for (int z = 0; z < 3; ++z) {
    for (int x = 0; x < 4; ++x) {
      glm::mat4 model =
        glm::translate(glm::mat4(1.0f), glm::vec3(x * 4.0f, 1.0f, z * 4.0f));
      transforms.push_back(model);
    }
  }

  glm::mat4 ground =
    glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, -1.0f, 6.0f)) *
    glm::scale(glm::mat4(1.0f), glm::vec3(24.0f, 1.0f, 24.0f));
  transforms.insert(transforms.begin(), ground);
}
