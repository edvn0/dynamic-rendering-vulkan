#include "scene/scene.hpp"

#include "core/app.hpp"
#include "renderer/renderer.hpp"
#include "scene/components.hpp"

// clang-format off
#include <imgui.h>
#include <ImGuizmo.h>
// clang-format on

#include "assets/manager.hpp"
#include "renderer/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <utility>

#include "renderer/editor_camera.hpp"
#include "renderer/layer.hpp"
#include "window/swapchain.hpp"

Scene::Scene(const std::string_view n)
  : scene_name(n) {};

auto
Scene::create_entity(std::string_view n) -> Entity
{
  std::string final_name = n.empty() ? "Entity" : std::string(n);
  if (!tag_registry.is_unique(final_name))
    final_name = tag_registry.generate_unique_name(final_name);
  const entt::entity handle = registry.create();
  tag_registry.register_tag(final_name, handle);

  const auto unique_id = tag_registry.get_unique_id(final_name).value();

  registry.emplace<Component::Tag>(handle, final_name, unique_id);
  registry.emplace<Component::Transform>(handle);
  return { handle, this };
}

auto
Scene::create_entt_entity() -> entt::entity
{
  return registry.create();
}

auto
Scene::on_update(double dt) -> void
{
  update_fly_controllers(dt);

  for (const auto view = registry.view<Component::Camera>();
       auto&& [entity, camera] : view.each()) {
    if (camera.clean())
      return;

    camera.projection = Camera::make_float_far_proj(
      camera.fov, camera.aspect, camera.znear, camera.zfar);
    camera.inverse_projection = Camera::make_float_far_proj(
      camera.fov, camera.aspect, camera.zfar, camera.znear);

    camera.dirty = true;
  }
}

// Helper function for fancy vector sliders
bool
Scene::draw_vector3_slider(const char* label,
                           glm::vec3& value,
                           float v_min = -100.0f,
                           float v_max = 100.0f,
                           const char* format = "%.3f")
{
  bool modified = false;

  ImGui::PushID(label);
  ImGui::Text("%s", label);
  ImGui::SameLine();

  // Color-coded component sliders
  ImGui::PushItemWidth(
    (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) /
    3);

  // X component (Red)
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.1f, 0.1f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
  if (ImGui::DragFloat("##X", &value.x, 0.01f, v_min, v_max, format))
    modified = true;
  ImGui::PopStyleColor(3);

  ImGui::SameLine();

  // Y component (Green)
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.4f, 0.1f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
  if (ImGui::DragFloat("##Y", &value.y, 0.01f, v_min, v_max, format))
    modified = true;
  ImGui::PopStyleColor(3);

  ImGui::SameLine();

  // Z component (Blue)
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.4f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.2f, 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(0.3f, 0.3f, 1.0f, 1.0f));
  if (ImGui::DragFloat("##Z", &value.z, 0.01f, v_min, v_max, format))
    modified = true;
  ImGui::PopStyleColor(3);

  ImGui::PopItemWidth();
  ImGui::PopID();

  return modified;
}

// Helper function for fancy vector4 sliders
bool
Scene::draw_vector4_slider(const char* label,
                           glm::vec4& value,
                           float v_min = -100.0f,
                           float v_max = 100.0f,
                           const char* format = "%.3f")
{
  bool modified = false;

  ImGui::PushID(label);
  ImGui::Text("%s", label);
  ImGui::SameLine();

  ImGui::PushItemWidth(
    (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3) /
    4);

  // X component (Red)
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.1f, 0.1f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
  if (ImGui::DragFloat("##X", &value.x, 0.01f, v_min, v_max, format))
    modified = true;
  ImGui::PopStyleColor(3);

  ImGui::SameLine();

  // Y component (Green)
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.4f, 0.1f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
  if (ImGui::DragFloat("##Y", &value.y, 0.01f, v_min, v_max, format))
    modified = true;
  ImGui::PopStyleColor(3);

  ImGui::SameLine();

  // Z component (Blue)
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.4f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.2f, 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(0.3f, 0.3f, 1.0f, 1.0f));
  if (ImGui::DragFloat("##Z", &value.z, 0.01f, v_min, v_max, format))
    modified = true;
  ImGui::PopStyleColor(3);

  ImGui::SameLine();

  // W component (Purple/Alpha)
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.1f, 0.4f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.9f, 0.2f, 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(1.0f, 0.3f, 1.0f, 1.0f));
  if (ImGui::DragFloat("##W", &value.w, 0.01f, v_min, v_max, format))
    modified = true;
  ImGui::PopStyleColor(3);

  ImGui::PopItemWidth();
  ImGui::PopID();

  return modified;
}

// Helper function for quaternion sliders with euler angle display
bool
Scene::draw_quaternion_slider(const char* label, glm::quat& quaternion)
{
  bool modified = false;

  ImGui::PushID(label);

  // Convert quaternion to Euler angles for easier editing
  glm::vec3 euler = glm::degrees(glm::eulerAngles(quaternion));

  // Wrap angles to [-180, 180] range
  for (int i = 0; i < 3; ++i) {
    while (euler[i] > 180.0f)
      euler[i] -= 360.0f;
    while (euler[i] < -180.0f)
      euler[i] += 360.0f;
  }

  // Draw euler angle sliders
  ImGui::Text("%s (Euler)", label);
  ImGui::SameLine();

  ImGui::PushItemWidth(
    (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) /
    3);

  // Pitch (X) - Red
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.1f, 0.1f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
  if (ImGui::DragFloat("##Pitch", &euler.x, 0.5f, -180.0f, 180.0f, "%.1f°"))
    modified = true;
  ImGui::PopStyleColor(3);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Pitch (X-axis rotation)");

  ImGui::SameLine();

  // Yaw (Y) - Green
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.4f, 0.1f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
  if (ImGui::DragFloat("##Yaw", &euler.y, 0.5f, -180.0f, 180.0f, "%.1f°"))
    modified = true;
  ImGui::PopStyleColor(3);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Yaw (Y-axis rotation)");

  ImGui::SameLine();

  // Roll (Z) - Blue
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.4f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.2f, 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                        ImVec4(0.3f, 0.3f, 1.0f, 1.0f));
  if (ImGui::DragFloat("##Roll", &euler.z, 0.5f, -180.0f, 180.0f, "%.1f°"))
    modified = true;
  ImGui::PopStyleColor(3);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Roll (Z-axis rotation)");

  ImGui::PopItemWidth();

  // Convert back to quaternion if modified
  if (modified) {
    quaternion = glm::quat(glm::radians(euler));
  }

  // Show quaternion components as well (read-only)
  ImGui::Indent();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
  ImGui::Text("Quaternion: x:%.3f y:%.3f z:%.3f w:%.3f",
              quaternion.x,
              quaternion.y,
              quaternion.z,
              quaternion.w);
  ImGui::PopStyleColor();
  ImGui::Unindent();

  ImGui::PopID();

  return modified;
}

// Enhanced entity display with icons and better formatting
void
Scene::draw_entity_item(entt::entity entity, const std::string_view tag)
{
  ImGui::PushID(static_cast<int>(entity));

  // Entity icon and name
  const bool is_selected = (selected_entity == entity);

  if (is_selected) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.6f, 0.9f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                          ImVec4(0.3f, 0.6f, 0.9f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          ImVec4(0.3f, 0.6f, 0.9f, 0.8f));
  }

  // Entity collapsing header
  bool expanded = ImGui::CollapsingHeader(tag.data());

  if (ImGui::IsItemClicked()) {
    selected_entity = entity;
  }

  if (is_selected) {
    ImGui::PopStyleColor(3);
  }

  // Show components when expanded
  if (expanded) {
    ImGui::Indent();

    // Transform component
    if (auto* transform = registry.try_get<Component::Transform>(entity)) {
      if (ImGui::TreeNode("Transform")) {

        // Position
        draw_vector3_slider("Position", transform->position, -1000.0f, 1000.0f);

        // Rotation (quaternion with euler angles)
        draw_quaternion_slider("Rotation", transform->rotation);

        // Scale
        draw_vector3_slider("Scale", transform->scale, 0.001f, 100.0f);
        ImGui::TreePop();
      }
    }

    // Mesh component
    if (auto* mesh = registry.try_get<Component::Mesh>(entity)) {
      if (ImGui::TreeNode("Mesh")) {
        // ImGui::Text("Mesh Asset: %p", static_cast<const void*>(mesh->mesh));
        ImGui::Checkbox("Casts Shadows", &mesh->casts_shadows);
        ImGui::Checkbox("Draw AABBs", &mesh->draw_aabb);
        ImGui::TreePop();
      }
    }

    if (auto* material = registry.try_get<Component::Material>(entity)) {
      auto& mat_data = material->material.get()->get_material_data();
      if (ImGui::TreeNode("Material")) {
        draw_vector4_slider("Albedo", mat_data.albedo, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &mat_data.roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Metallic", &mat_data.metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("AO", &mat_data.ao, 0.0f, 1.0f);

        ImGui::SliderFloat(
          "Emissive Strength", &mat_data.emissive_strength, 0.0f, 10.0f);
        draw_vector3_slider(
          "Emissive Color", mat_data.emissive_color, 0.0f, 10.0f);

        ImGui::SliderFloat("Clearcoat", &mat_data.clearcoat, 0.0f, 1.0f);
        ImGui::SliderFloat(
          "Clearcoat Roughness", &mat_data.clearcoat_roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Anisotropy", &mat_data.anisotropy, 0.0f, 1.0f);
        ImGui::SliderFloat("Alpha Cutoff", &mat_data.alpha_cutoff, 0.0f, 1.0f);

        ImGui::TreePop();
      }
    }

    if (auto* camera = registry.try_get<Component::Camera>(entity)) {
      if (ImGui::TreeNode("Camera")) {
        bool modified = false;

        modified |= ImGui::SliderFloat("FOV", &camera->fov, 1.0f, 179.0f);
        modified |= ImGui::SliderFloat("Aspect", &camera->aspect, 0.1f, 5.0f);
        modified |= ImGui::SliderFloat("Z Near", &camera->znear, 0.01f, 10.0f);
        modified |= ImGui::SliderFloat("Z Far", &camera->zfar, 10.0f, 10000.0f);

        if (modified) {
          camera->dirty = true;
        }

        ImGui::TreePop();
      }
    }

    // Add component button
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
      Entity e{ entity, this };
      if (ImGui::MenuItem("Mesh")) {
      }
      if (ImGui::MenuItem("Light")) {
      }
      if (ImGui::MenuItem("Camera")) {
      }
      if (ImGui::MenuItem("Material")) {
        auto material =
          Assets::Manager::the().load<::Material>("main_geometry");
        e.add_component<Component::Material>(material);
      }
      ImGui::EndPopup();
    }

    ImGui::Unindent();
  }

  ImGui::PopID();
}

auto
Scene::on_interface() -> void
{
  ImGui::Begin(scene_name.data(), nullptr, ImGuiWindowFlags_MenuBar);

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("Entity")) {
      if (ImGui::MenuItem("Create Entity", "Ctrl+N")) {
        create_entity("New Entity");
      }
      if (ImGui::MenuItem("Delete Selected",
                          "Delete",
                          false,
                          selected_entity != entt::null)) {
        registry.destroy(selected_entity);
        selected_entity = entt::null;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Show Components", nullptr, &show_components);
      ImGui::MenuItem("Show Statistics", nullptr, &show_statistics);
      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  if (show_statistics) {
    auto entity_count = registry.view<const Component::Tag>().size();
    auto mesh_count = registry.view<const Component::Mesh>().size();

    ImGui::Text("Entities: %zu | Meshes: %zu", entity_count, mesh_count);
    ImGui::Separator();
  }

  ImGui::Text("🏷️ Entities");
  ImGui::Separator();

  static std::array<char, 256> search_buffer{};
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##search",
                           "🔍 Search entities...",
                           search_buffer.data(),
                           sizeof(search_buffer));

  std::string search_term = search_buffer.data();
  std::ranges::transform(search_term, search_term.begin(), ::tolower);

  ImGui::BeginChild("EntityList", ImVec2(0, 0), true);

  for (auto&& [entity, tag] : registry.view<Component::Tag>().each()) {
    if (!search_term.empty()) {
      std::string entity_name = tag.name;
      std::ranges::transform(entity_name, entity_name.begin(), ::tolower);
      if (!entity_name.contains(search_term)) {
        continue;
      }
    }

    draw_entity_item(entity, tag.name);
  }

  ImGui::EndChild();

  ImGui::End();

  if (scene_camera_entity.valid()) {
    auto* camera = scene_camera_entity.try_get<Component::Camera>();

    if (auto* transform = scene_camera_entity.try_get<Component::Transform>();
        camera && transform) {
      glm::mat4 view = glm::inverse(transform->compute());
      const glm::mat4& projection = camera->projection;

      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

      ImGuizmo::SetRect(vp_min.x, vp_min.y, vp_max.x, vp_max.y);

      glm::mat4 model = transform->compute();

      ImGuizmo::Manipulate(glm::value_ptr(view),
                           glm::value_ptr(projection),
                           ImGuizmo::OPERATION::TRANSLATE,
                           ImGuizmo::LOCAL,
                           glm::value_ptr(model));

      if (ImGuizmo::IsUsing()) {
        glm::vec3 skew{};
        glm::vec3 translation{};
        glm::vec3 scale{};
        glm::vec4 perspective{};
        glm::quat rotation{};

        if (glm::decompose(
              model, scale, rotation, translation, skew, perspective)) {
          constexpr float scale_min = 0.001f;
          scale.x = glm::max(glm::abs(scale.x), scale_min) * glm::sign(scale.x);
          scale.y = glm::max(glm::abs(scale.y), scale_min) * glm::sign(scale.y);
          scale.z = glm::max(glm::abs(scale.z), scale_min) * glm::sign(scale.z);

          if (!glm::any(glm::isnan(rotation)) &&
              glm::length2(rotation) > 0.0001f)
            rotation = glm::normalize(rotation);
          else
            rotation = glm::quat_identity<float, glm::defaultp>();

          transform->position = translation;
          transform->rotation = rotation;
          transform->scale = scale;
        }
      }
    }
  }

  if (show_components && selected_entity != entt::null) {
    ImGui::Begin("Inspector", &show_components);

    if (auto tag = registry.try_get<Component::Tag>(selected_entity)) {
      ImGui::Text("Entity: %s", tag->name.c_str());
      ImGui::Text("ID: %u", std::to_underlying(selected_entity));
      ImGui::Separator();

      draw_entity_item(selected_entity, tag->name);
    }

    ImGui::End();
  }
}

auto
Scene::on_render(Renderer& renderer) -> void
{
  for (const auto view =
         registry.view<Component::Mesh, const Component::Transform>();
       auto&& [entity, mesh, transform] : view.each()) {
    const auto* material_component =
      registry.try_get<Component::Material>(entity);
    const auto* identifier = registry.try_get<Component::Tag>(entity);
    Assets::Handle<Material> mat{};
    if (material_component) {
      mat = material_component->material;
    }

    const auto actual_mesh = mesh.mesh.get();
    renderer.submit(
      {
        .mesh = actual_mesh,
        .override_material = mat,
        .casts_shadows = mesh.casts_shadows,
      },
      transform.compute(),
      identifier->identifier);

    if (mesh.draw_aabb) {
      std::ranges::for_each(
        actual_mesh->get_submeshes(),
        [&m = actual_mesh, &r = renderer](const auto& submesh) {
          r.submit_aabb(m->get_world_aabb(submesh));
        });
    }
  }
}
auto
Scene::on_event(Event& event) -> bool
{
  // Delete
  EventDispatcher dispatcher(event);
  dispatcher.dispatch<KeyReleasedEvent>([this](const KeyReleasedEvent& ev) {
    if (selected_is_valid() && KeyCode::Delete == ev.key) {
      delete_entity(selected_entity);
    }
    return true;
  });

  return true;
}

auto
Scene::selected_is_valid() const -> bool
{
  return registry.valid(selected_entity);
}

auto
Scene::on_resize(const EditorCamera& camera, std::uint32_t w, std::uint32_t h)
  -> void
{
  if (auto* cam = scene_camera_entity.try_get<Component::Camera>();
      nullptr != cam) {
    cam->on_resize(calculate_aspect_ratio<float>(w, h));
  }

  scene_camera_entity.get_component<Component::Transform>().position =
    camera.get_position();
}

auto
Scene::update_viewport_bounds(const DynamicRendering::ViewportBounds& bounds)
  -> void
{
  vp_min = bounds.min;
  vp_max = bounds.max;
}

auto
Scene::delete_entity(const entt::entity to_delete) -> void
{
  if (!registry.valid(to_delete)) {
    Logger::log_warning("Input entity: {} was not valid.",
                        entt::to_integral(to_delete));
    return;
  }

  const auto version = registry.destroy(to_delete);
  (void)version;
  if (to_delete == selected_entity) {
    selected_entity = entt::null;
  }
}

auto
Scene::on_initialise(const InitialisationParameters& parameters) -> void
{
  scene_camera_entity = create_entity("SceneCamera");
  scene_camera_entity.add_component<Component::FlyController>();
  auto& camera = scene_camera_entity.add_component<Component::Camera>();
  camera.aspect = static_cast<float>(parameters.swapchain.get_width()) /
                  static_cast<float>(parameters.swapchain.get_height());
  camera.dirty = true;
  auto& transform = scene_camera_entity.get_component<Component::Transform>();
  transform.position = parameters.camera.get_position();
}

void
Scene::update_fly_controllers(double delta_seconds)
{

  for (auto view =
         registry.view<Component::Transform, Component::FlyController>();
       auto&& [entity, transform, controller] : view.each()) {
    if (!controller.active)
      continue;

    glm::vec3 direction{ 0.0f };
    if (Input::key_pressed(KeyCode::W))
      direction += glm::vec3{ 0, 0, -1 };
    if (Input::key_pressed(KeyCode::S))
      direction += glm::vec3{ 0, 0, 1 };
    if (Input::key_pressed(KeyCode::A))
      direction += glm::vec3{ -1, 0, 0 };
    if (Input::key_pressed(KeyCode::D))
      direction += glm::vec3{ 1, 0, 0 };
    if (Input::key_pressed(KeyCode::Space))
      direction += glm::vec3{ 0, 1, 0 };
    if (Input::key_pressed(KeyCode::LeftShift))
      direction += glm::vec3{ 0, -1, 0 };

    if (glm::length2(direction) > 0.0f) {
      glm::vec3 forward = transform.rotation * glm::vec3{ 0, 0, -1 };
      glm::vec3 right = transform.rotation * glm::vec3{ 1, 0, 0 };
      glm::vec3 up = transform.rotation * glm::vec3{ 0, 1, 0 };

      glm::vec3 move_vector =
        direction.z * forward + direction.x * right + direction.y * up;
      transform.position +=
        move_vector * controller.move_speed * static_cast<float>(delta_seconds);
    }

    if (Input::mouse_pressed(MouseCode::MouseButtonRight)) {
      static float last_x = 0, last_y = 0;
      auto [x, y] = Input::mouse_position();
      float dx = static_cast<float>(x) - last_x;
      float dy = static_cast<float>(y) - last_y;
      last_x = static_cast<float>(x);
      last_y = static_cast<float>(y);

      glm::quat yaw = glm::angleAxis(
        glm::radians(-dx * controller.rotation_speed), glm::vec3{ 0, 1, 0 });
      glm::quat pitch = glm::angleAxis(
        glm::radians(-dy * controller.rotation_speed), glm::vec3{ 1, 0, 0 });

      transform.rotation = glm::normalize(yaw * transform.rotation * pitch);
    }
  }
}
