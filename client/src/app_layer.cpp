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

struct CubeComponent
{
  static constexpr auto name = "CubeComponent";
  bool is_active{ true };
};

AppLayer::AppLayer(const Device& dev,
                   BS::priority_thread_pool* pool,
                   BlueprintRegistry* registry)
  : thread_pool(pool)
  , blueprint_registry(registry)
{
  active_scene = std::make_shared<Scene>("Basic");

  auto tokyo_entity = active_scene->create_entity("Tokyo");

  // assert(mesh->load_from_file(dev, *blueprint_registry,
  // "cerberus/scene.gltf"));

  tokyo_mesh->load_from_file(
    dev, *blueprint_registry, pool, "little_tokyo/scene.gltf");
  tokyo_entity.add_component<Component::Mesh>(tokyo_mesh.get(), false);
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

  active_scene->on_interface();

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
  active_scene->on_update(ts);

  static float angle_deg = 0.f;
  angle_deg += static_cast<float>(ts) * rotation_speed;
  angle_deg = fmod(angle_deg, 360.f);

  bool first = true;
  active_scene->each<const CubeComponent, Component::Transform>(
    [&](auto, const auto&, Component::Transform& transform) {
      if (first) {
        first = false;
        return;
      }

      transform.rotation =
        glm::angleAxis(glm::radians(angle_deg), glm::vec3(0.f, 1.f, 0.f));
    });
}

auto
AppLayer::on_render(Renderer& renderer) -> void
{
  ZoneScopedN("App on_render");
  active_scene->on_render(renderer);

  auto& light_env = renderer.get_light_environment();
  light_env.light_position = light_position;
  light_env.light_color = light_color;

  renderer.submit(
    {
      .mesh = mesh.get(),
    },
    glm::mat4(1.f));

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
}

auto
AppLayer::on_resize(std::uint32_t w, std::uint32_t h) -> void
{
  bounds.x = static_cast<float>(w);
  bounds.y = static_cast<float>(h);

  active_scene->on_resize(app->get_editor_camera(), w, h);
}

auto
AppLayer::on_initialise(const InitialisationParameters& params) -> void
{
  app = &params.app;
  active_scene->on_initialise(params);
}

auto
AppLayer::generate_scene() -> void
{
  auto maybe_cube_mesh = MeshCache::the().get_mesh<MeshType::Cube>();
  if (!maybe_cube_mesh.has_value()) {
    return;
  }

  auto cube_mesh = maybe_cube_mesh.value();

  {
    auto entity = active_scene->create_entity("Ground");
    auto& transform = entity.get_component<Component::Transform>();
    entity.add_component<Component::Mesh>(cube_mesh, true);
    transform.position = glm::vec3(6.0f, -1.0f, 6.0f);
    transform.scale = glm::vec3(24.0f, 1.0f, 24.0f);
  }

  for (int z = 0; z < 3; ++z) {
    for (int x = 0; x < 4; ++x) {
      auto entity =
        active_scene->create_entity(std::format("Cube_{}_{}", x, z));
      entity.add_component<Component::Mesh>(cube_mesh, true);
      entity.add_component<CubeComponent>();
      auto& transform = entity.get_component<Component::Transform>();
      transform.position = glm::vec3(x * 4.0f, 1.0f, z * 4.0f);
    }
  }
}

auto
AppLayer::on_ray_pick(const glm::vec3& origin, const glm::vec3& direction)
  -> void
{
  if (!active_scene)
    return;

  float closest_distance = std::numeric_limits<float>::max();
  entt::entity closest_entity = entt::null;

  active_scene->each<const CubeComponent, Component::Transform>(
    [&](entt::entity entity,
        const CubeComponent&,
        const Component::Transform& transform) {
      const glm::mat4 model = transform.compute();
      const glm::vec3 aabb_min(-0.5f), aabb_max(0.5f);

      const glm::mat4 inv_model = glm::inverse(model);
      glm::vec3 ray_origin_local =
        glm::vec3(inv_model * glm::vec4(origin, 1.0f));
      glm::vec3 ray_dir_local =
        glm::normalize(glm::vec3(inv_model * glm::vec4(direction, 0.0f)));

      float tmin = 0.0f, tmax = 10000.0f;

      for (int i = 0; i < 3; ++i) {
        float inv_d = 1.0f / ray_dir_local[i];
        float t0 = (aabb_min[i] - ray_origin_local[i]) * inv_d;
        float t1 = (aabb_max[i] - ray_origin_local[i]) * inv_d;
        if (inv_d < 0.0f)
          std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin)
          return; // correct: just skip this entity
      }

      if (tmin < closest_distance) {
        closest_distance = tmin;
        closest_entity = entity;
      }
    });

  if (closest_entity != entt::null) {
    active_scene->set_selected_entity(closest_entity);
  }
}
