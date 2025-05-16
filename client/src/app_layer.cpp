#include "app_layer.hpp"

#include <array>
#include <execution>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <numeric>
#include <span>

#include <imgui.h>
#include <latch>
#include <random>
#include <tracy/Tracy.hpp>

AppLayer::AppLayer(const Device& dev,
                   BS::priority_thread_pool* pool,
                   BlueprintRegistry* registry)
  : thread_pool(pool)
  , blueprint_registry(registry)
{
  active_scene = std::make_shared<Scene>("Basic");

  auto tokyo_entity = active_scene->create_entity("Tokyo");
  tokyo_entity.add_component<Component::Mesh>();

  // assert(mesh->load_from_file(dev, *blueprint_registry,
  // "cerberus/scene.gltf"));

  tokyo_mesh->load_from_file(
    dev, *blueprint_registry, pool, "little_tokyo/scene.gltf");
  armour_mesh->load_from_file(
    dev, *blueprint_registry, "battle_armour/scene.gltf");
  hunter_mesh->load_from_file(
    dev, *blueprint_registry, "astro_hunter_special/scene.gltf");

  generate_scene();
}

AppLayer::~AppLayer() = default;

auto
AppLayer::on_destroy() -> void
{
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

  renderer.submit(
    {
      .mesh = tokyo_mesh.get(),
    },
    glm::translate(glm::mat4(1.f), { 0.f, -1.f, 0.f }) *
      glm::rotate(
        glm::mat4{ 1.f }, glm::radians(90.F), glm::vec3(0.0F, 1.0F, 0.0F)) *
      glm::scale(glm::mat4(1.f), { 0.01f, 0.01f, 0.01f }));
  renderer.submit(
    {
      .mesh = armour_mesh.get(),
    },
    glm::translate(glm::mat4(1.f), { 0.f, -1.f, 0.f }) *
      glm::rotate(
        glm::mat4{ 1.f }, glm::radians(90.F), glm::vec3(0.0F, 1.0F, 0.0F)));
  renderer.submit(
    {
      .mesh = hunter_mesh.get(),
    },
    glm::translate(glm::mat4(1.f), { 0.f, -1.f, 0.f }) *
      glm::rotate(
        glm::mat4{ 1.f }, glm::radians(90.F), glm::vec3(0.0F, 1.0F, 0.0F)));

  static constexpr float line_width = 1.2f;
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
