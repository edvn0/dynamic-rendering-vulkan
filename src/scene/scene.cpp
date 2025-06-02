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

Scene::Scene(const std::string_view n)
  : scene_name(n) {};

Entity
Scene::create_entity(std::string_view n)
{
  std::string final_name = n.empty() ? "Entity" : std::string(n);
  if (!tag_registry.is_unique(final_name))
    final_name = tag_registry.generate_unique_name(final_name);
  const entt::entity handle = registry.create();
  tag_registry.register_tag(final_name, handle);
  registry.emplace<Component::Tag>(handle, final_name);
  registry.emplace<Component::Transform>(handle);
  return { handle, this };
}

auto
Scene::create_entt_entity() -> entt::entity
{
  return registry.create();
}

auto
Scene::on_update(double) -> void
{
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
  bool is_selected = (selected_entity == entity);

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

    // Add component button
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
      if (ImGui::MenuItem("Mesh")) {
      }
      if (ImGui::MenuItem("Light")) {
      }
      if (ImGui::MenuItem("Camera")) {
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

  if (selected_entity != entt::null) {
    auto* transform = registry.try_get<Component::Transform>(selected_entity);
    if (auto* scene_camera =
          scene_camera_entity.try_get<SceneCameraComponent>();
        transform && scene_camera) {
      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

      ImGuizmo::SetRect(vp_min.x, vp_min.y, vp_max.x, vp_max.y);

      glm::mat4 view = scene_camera->view;
      glm::mat4 proj = scene_camera->projection;
      proj[1][1] *= -1.0f;
      glm::mat4 model = transform->compute();

      ImGuizmo::Manipulate(glm::value_ptr(view),
                           glm::value_ptr(proj),
                           ImGuizmo::OPERATION::TRANSLATE, // or ROTATE/SCALE
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
    const auto actual_mesh = Assets::Manager::the().get(mesh.mesh);
    renderer.submit(
      {
        .mesh = actual_mesh,
        .casts_shadows = mesh.casts_shadows,
      },
      transform.compute());

    if (mesh.draw_aabb) {
      std::ranges::for_each(
        actual_mesh->get_submeshes(),
        [&scale = transform.scale, &m = actual_mesh, &r = renderer](
          const auto& submesh) { r.submit_aabb(m->get_world_aabb(submesh)); });
    }
  }
}

auto
Scene::on_resize(const EditorCamera& camera, std::uint32_t, std::uint32_t)
  -> void
{
  auto& [position, view, projection] =
    scene_camera_entity.get_component<SceneCameraComponent>();
  projection = camera.get_projection();
  view = camera.get_view();
  position = camera.get_position();
}

auto
Scene::update_viewport_bounds(const DynamicRendering::ViewportBounds& bounds)
  -> void
{
  vp_min = bounds.min;
  vp_max = bounds.max;
}

auto
Scene::on_initialise(const InitialisationParameters& params) -> void
{
  scene_camera_entity = create_entity("SceneCamera");
  scene_camera_entity.add_component<SceneCameraComponent>(
    params.camera.get_position(),
    params.camera.get_view(),
    params.camera.get_projection());
}