#include "app_layer.hpp"

#include "dynamic_rendering/assets/manager.hpp"

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

AppLayer::AppLayer(const Device&,
                   BS::priority_thread_pool* pool,
                   BlueprintRegistry* registry)
  : thread_pool(pool)
  , blueprint_registry(registry)
{
  active_scene = std::make_shared<Scene>("Basic");

  auto tokyo_entity = active_scene->create_entity("Tokyo");

  Assets::Manager::the().load<Image>("sf.ktx2");
  auto handle =
    Assets::Manager::the().load<StaticMesh>("little_tokyo/scene.gltf");
  tokyo_entity.add_component<Component::Mesh>(handle, false);

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

  window("Controls", [&, this]() {
    ImGui::SliderFloat("Rotation Speed", &rotation_speed, 0.1f, 150.0f);

    ImGui::DragFloat3(
      "Light Position", &light_environment.light_position[0], 0.5f);
    ImGui::DragFloat3("Light Target", &light_environment.target[0], 0.5f);
    ImGui::ColorEdit3("Light Color", &light_environment.light_color[0]);
    ImGui::ColorEdit3("Ambient Color", &light_environment.ambient_color[0]);

    ImGui::DragFloat(
      "Ortho Size", &light_environment.ortho_size, 0.5f, 1.f, 200.f);
    ImGui::DragFloat(
      "Near Plane", &light_environment.near_plane, 0.01f, 0.01f, 10.f);
    ImGui::DragFloat(
      "Far Plane", &light_environment.far_plane, 0.1f, 1.f, 500.f);

    static const char* view_mode_names[] = { "LookAtRH", "LookAtLH" };
    static const char* projection_names[] = {
      "OrthoRH_ZO", "OrthoRH_NO", "OrthoLH_ZO", "OrthoLH_NO"
    };
    int proj_index = static_cast<int>(light_environment.projection_mode);
    if (ImGui::Combo("Projection Mode",
                     &proj_index,
                     projection_names,
                     IM_ARRAYSIZE(projection_names))) {
      light_environment.projection_mode =
        static_cast<ShadowProjectionMode>(proj_index);
    }

    int view_index = static_cast<int>(light_environment.view_mode);
    if (ImGui::Combo("View Mode",
                     &view_index,
                     view_mode_names,
                     IM_ARRAYSIZE(view_mode_names))) {
      light_environment.view_mode = static_cast<ShadowViewMode>(view_index);
    }
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

  active_scene->each<const CubeComponent, Component::Transform>(
    [&](auto, const auto&, Component::Transform& transform) {
      transform.rotation =
        glm::angleAxis(glm::radians(angle_deg), glm::vec3(0.f, 1.f, 0.f));
    });
}

auto
AppLayer::on_render(Renderer& renderer) -> void
{
  ZoneScopedN("App on_render");
  active_scene->on_render(renderer);

  renderer.get_light_environment() = light_environment;

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
  {
    auto entity = active_scene->create_entity("Ground");
    auto& transform = entity.get_component<Component::Transform>();
    entity.add_component<Component::Mesh>(Assets::builtin_cube(), true);
    transform.position = glm::vec3(6.0f, -1.0f, 6.0f);
    transform.scale = glm::vec3(24.0f, 1.0f, 24.0f);
  }

  for (int z = 0; z < 3; ++z) {
    for (int x = 0; x < 4; ++x) {
      auto entity =
        active_scene->create_entity(std::format("Cube_{}_{}", x, z));
      entity.add_component<Component::Mesh>(Assets::builtin_cube(), true);
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

  active_scene->each<const CubeComponent, const Component::Transform>(
    [&](const entt::entity entity,
        const CubeComponent&,
        const Component::Transform& transform) {
      const glm::mat4 model = transform.compute();
      const glm::mat4 inv_model = glm::inverse(model);

      const auto ray_origin_local =
        glm::vec3(inv_model * glm::vec4(origin, 1.0f));
      const auto ray_dir_local =
        glm::normalize(glm::vec3(inv_model * glm::vec4(direction, 0.0f)));

      constexpr glm::vec3 aabb_min(-0.5f), aabb_max(0.5f);
      float tmin = -std::numeric_limits<float>::max();
      float tmax = std::numeric_limits<float>::max();

      for (int i = 0; i < 3; ++i) {
        const float dir_component = ray_dir_local[i];
        const float orig_component = ray_origin_local[i];

        if (std::abs(dir_component) < 1e-6f) {
          if (orig_component < aabb_min[i] || orig_component > aabb_max[i])
            return; // Parallel and outside slab
          continue;
        }

        const float inv_d = 1.0f / dir_component;
        float t0 = (aabb_min[i] - orig_component) * inv_d;
        float t1 = (aabb_max[i] - orig_component) * inv_d;

        if (inv_d < 0.0f)
          std::swap(t0, t1);

        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin)
          return;
      }

      if (tmin < closest_distance && tmax >= 0.0f) {
        closest_distance = tmin;
        closest_entity = entity;
      }
    });

  if (closest_entity != entt::null) {
    const ReadonlyEntity entity{ closest_entity, active_scene.get() };
    if (auto* tag = entity.try_get<const Component::Tag>()) {
      Logger::log_info("{}", tag->name);
      active_scene->set_selected_entity(closest_entity);
    }
  }
}
