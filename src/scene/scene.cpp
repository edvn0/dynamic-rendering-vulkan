#include "scene/scene.hpp"

#include "renderer/renderer.hpp"
#include "scene/components.hpp"

// clang-format off
#include <imgui.h>
#include <ImGuizmo.h>
// clang-format on

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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
  return Entity(handle, this);
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
    if (auto transform = registry.try_get<Component::Transform>(entity)) {
      if (ImGui::TreeNode("Transform")) {

        // Position
        draw_vector3_slider("Position", transform->position, -1000.0f, 1000.0f);

        // Rotation (quaternion with euler angles)
        draw_quaternion_slider("Rotation", transform->rotation);

        // Scale
        draw_vector3_slider("Scale", transform->scale, 0.001f, 100.0f);

        // Show computed matrix info
        ImGui::Separator();
        ImGui::Text("Transform Matrix:");
        auto matrix = transform->compute();
        ImGui::Text("[ %.2f %.2f %.2f %.2f ]",
                    matrix[0][0],
                    matrix[1][0],
                    matrix[2][0],
                    matrix[3][0]);
        ImGui::Text("[ %.2f %.2f %.2f %.2f ]",
                    matrix[0][1],
                    matrix[1][1],
                    matrix[2][1],
                    matrix[3][1]);
        ImGui::Text("[ %.2f %.2f %.2f %.2f ]",
                    matrix[0][2],
                    matrix[1][2],
                    matrix[2][2],
                    matrix[3][2]);
        ImGui::Text("[ %.2f %.2f %.2f %.2f ]",
                    matrix[0][3],
                    matrix[1][3],
                    matrix[2][3],
                    matrix[3][3]);

        ImGui::TreePop();
      }
    }

    // Mesh component
    if (auto mesh = registry.try_get<Component::Mesh>(entity)) {
      if (ImGui::TreeNode("Mesh")) {
        ImGui::Text("Mesh Asset: %p", (const void*)mesh->mesh);
        ImGui::Checkbox("Casts Shadows", &mesh->casts_shadows);
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
        // Add mesh component logic
      }
      if (ImGui::MenuItem("Light")) {
        // Add light component logic
      }
      if (ImGui::MenuItem("Camera")) {
        // Add camera component logic
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
  // Main scene window
  ImGui::Begin(scene_name.data(), nullptr, ImGuiWindowFlags_MenuBar);

  // Menu bar
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

  // Scene statistics
  if (show_statistics) {
    auto entity_count = registry.view<Component::Tag>().size();
    auto mesh_count = registry.view<Component::Mesh>().size();

    ImGui::Text("Entities: %zu | Meshes: %zu", entity_count, mesh_count);
    ImGui::Separator();
  }

  // Entity list
  ImGui::Text("🏷️ Entities");
  ImGui::Separator();

  // Search filter
  static char search_buffer[256] = "";
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint(
    "##search", "🔍 Search entities...", search_buffer, sizeof(search_buffer));

  // Entity list with filtering
  std::string search_term = search_buffer;
  std::transform(
    search_term.begin(), search_term.end(), search_term.begin(), ::tolower);

  ImGui::BeginChild("EntityList", ImVec2(0, 0), true);

  for (auto&& [entity, tag] : registry.view<Component::Tag>().each()) {
    // Filter entities based on search
    if (!search_term.empty()) {
      std::string entity_name = tag.name;
      std::transform(
        entity_name.begin(), entity_name.end(), entity_name.begin(), ::tolower);
      if (entity_name.find(search_term) == std::string::npos) {
        continue;
      }
    }

    draw_entity_item(entity, tag.name);
  }

  ImGui::EndChild();

  ImGui::End();

  if (selected_entity != entt::null) {
    auto* transform = registry.try_get<Component::Transform>(selected_entity);
    auto* scene_camera = scene_camera_entity.try_get<SceneCameraComponent>();
    if (transform) {
      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist();

      auto window_pos = ImGui::GetMainViewport()->Pos;
      auto window_size = ImGui::GetMainViewport()->Size;
      ImGuizmo::SetRect(
        window_pos.x, window_pos.y, window_size.x, window_size.y);

      glm::mat4 view = scene_camera->view;
      glm::mat4 proj = scene_camera->projection;
      glm::mat4 model = transform->compute();

      ImGuizmo::Manipulate(glm::value_ptr(view),
                           glm::value_ptr(proj),
                           ImGuizmo::OPERATION::TRANSLATE, // or ROTATE/SCALE
                           ImGuizmo::LOCAL,
                           glm::value_ptr(model));

      if (ImGuizmo::IsUsing()) {
        glm::vec3 skew, translation, scale;
        glm::vec4 perspective;
        glm::quat rotation;

        if (glm::decompose(
              model, scale, rotation, translation, skew, perspective)) {
          const float scale_min = 0.001f;
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
      ImGui::Text("ID: %u", static_cast<uint32_t>(selected_entity));
      ImGui::Separator();

      draw_entity_item(selected_entity, tag->name);
    }

    ImGui::End();
  }
}

auto
Scene::on_render(Renderer& renderer) -> void
{
  // Find all meshes+transforms in the scene
  auto view = registry.view<Component::Mesh, Component::Transform>();
  for (auto [entity, mesh, transform] : view.each()) {
    renderer.submit(
      {
        .mesh = mesh.mesh,
        .casts_shadows = mesh.casts_shadows,
      },
      transform.compute());
  }
}

auto
Scene::on_resize(const EditorCamera& camera, std::uint32_t, std::uint32_t)
  -> void
{
  auto& scene_camera =
    scene_camera_entity.get_component<SceneCameraComponent>();
  scene_camera.projection = camera.get_projection();
  scene_camera.view = camera.get_view();
  scene_camera.position = camera.get_position();
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