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
#include "window/window.hpp"

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
#include <tracy/Tracy.hpp>

Scene::Scene(const std::string_view n)
  : scene_name(n)
{
  scene_camera_entity = create_entity("SceneCamera");
  scene_camera_entity.add_component<Component::FlyController>();
  scene_camera_entity.add_component<Component::Camera>();
};

auto
Scene::create_entity(const std::string_view n) -> Entity
{
  std::string final_name = n.empty() ? "Entity" : std::string(n);
  if (!tag_registry.is_unique(final_name))
    final_name = tag_registry.generate_unique_name(final_name);
  const entt::entity handle = registry.create();
  tag_registry.register_tag(final_name, handle);

  const auto unique_id = tag_registry.get_unique_id(final_name).value();

  registry.emplace<Component::Tag>(handle, final_name, unique_id);
  registry.emplace<Component::Transform>(handle);

  registry.emplace<Component::Hierarchy>(handle);

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

  bool expanded = ImGui::CollapsingHeader(tag.data());

  if (ImGui::IsItemClicked()) {
    selected_entity = entity;
  }

  if (is_selected) {
    ImGui::PopStyleColor(3);
  }

  if (expanded) {
    ImGui::Indent();

    if (auto* transform = registry.try_get<Component::Transform>(entity)) {
      if (ImGui::TreeNode("Transform")) {
        draw_vector3_slider("Position", transform->position, -1000.0f, 1000.0f);
        draw_quaternion_slider("Rotation", transform->rotation);
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

        // Drop target
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("FILE_BROWSER_ENTRY")) {
            auto paths =
              std::filesystem::path(static_cast<const char*>(payload->Data));
            if (!std::filesystem::exists(paths)) {
              ImGui::EndDragDropTarget();
              ImGui::PopID();
              return;
            }
            std::filesystem::path found = paths;

            if (auto mesh_asset =
                  Assets::Manager::the().load<::StaticMesh>(found.string());
                mesh_asset.is_valid()) {
              mesh->mesh = mesh_asset;
            }
          }
          ImGui::EndDragDropTarget();
        }

        ImGui::TreePop();
      }
    }

    if (auto* mat_component = registry.try_get<Component::Material>(entity);
        mat_component) {
      auto* material = mat_component->material.get();
      auto& mat_data = material->get_material_data();
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

        ImGui::Separator();

        // --- Texture Maps Section ---
        if (ImGui::TreeNode("Texture Maps")) {
          constexpr ImVec2 preview_size{ 64.0f, 64.0f };

          auto draw_texture_slot = [&](const char* label,
                                       const char* slot_name,
                                       std::uint32_t flag,
                                       VkFormat format,
                                       const char* tooltip = nullptr) {
            ImGui::PushID(slot_name); // Unique ID scope for ImGui widgets
            ImGui::Text("%s:", label);

            const Image* current_image = nullptr;
            auto maybe_image = material->get_image(slot_name);
            current_image = maybe_image.has_value()
                              ? maybe_image.value()
                              : Renderer::get_white_texture();

            // Draw Image or Placeholder Button
            ImTextureID texture_id =
              *current_image->get_texture_id<ImTextureID>();
            std::string unique_id = std::string("##") + slot_name;

            if (texture_id) {
              ImGui::Image(texture_id, preview_size);
            } else {
              ImGui::Button(("Drop Image" + unique_id).c_str(), preview_size);
            }

            // Accept drag-drop payload
            if (ImGui::BeginDragDropTarget()) {
              if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("FILE_BROWSER_ENTRY")) {
                std::string buffer;
                buffer.resize(256, '\0');
                std::memcpy(
                  buffer.data(),
                  payload->Data,
                  std::min(static_cast<std::size_t>(payload->DataSize),
                           buffer.size() - 1));

                auto paths = std::filesystem::path(buffer);
                if (!std::filesystem::exists(paths)) {
                  ImGui::EndDragDropTarget();
                  ImGui::PopID();
                  return;
                }
                std::filesystem::path found = paths;

                SampledTextureImageConfiguration config;
                config.format = format;

                if (auto image =
                      Assets::Manager::the()
                        .load<::Image, SampledTextureImageConfiguration>(
                          found.string(), config);
                    image.is_valid()) {
                  material->upload(slot_name, image.get());
                  mat_data.flags |= flag;

                  if (flag == MaterialData::FLAG_EMISSIVE_MAP &&
                      !(mat_data.flags & MaterialData::FLAG_EMISSIVE)) {
                    mat_data.set_emissive(true);
                  }
                }
              }
              ImGui::EndDragDropTarget();
            }

            // Right-click to clear texture
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
              if (mat_data.flags & flag) {
                material->upload(slot_name, Renderer::get_white_texture());
                mat_data.flags &= ~flag;

                if (flag == MaterialData::FLAG_EMISSIVE_MAP) {
                  bool has_emissive_color = mat_data.emissive_color.x > 0.0f ||
                                            mat_data.emissive_color.y > 0.0f ||
                                            mat_data.emissive_color.z > 0.0f;
                  if (!has_emissive_color &&
                      mat_data.emissive_strength <= 0.0f) {
                    mat_data.set_emissive(false);
                  }
                }
              }
            }

            // Tooltip
            if (tooltip && ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%s\n\nRight-click to clear texture.", tooltip);
            }

            ImGui::Spacing();
            ImGui::PopID();
          };

          draw_texture_slot(
            "Albedo Map",
            "albedo_map",
            MaterialData::FLAG_ALBEDO_TEXTURE,
            VK_FORMAT_R8G8B8A8_SRGB,
            "Drag & drop albedo/diffuse texture here. Right-click to clear.");

          draw_texture_slot(
            "Normal Map",
            "normal_map",
            MaterialData::FLAG_NORMAL_MAP,
            VK_FORMAT_R8G8B8A8_UNORM,
            "Drag & drop normal map here. Right-click to clear.");

          draw_texture_slot(
            "Roughness Map",
            "roughness_map",
            MaterialData::FLAG_ROUGHNESS_MAP,
            VK_FORMAT_R8G8B8A8_UNORM,
            "Drag & drop roughness texture here. Right-click to clear.");

          draw_texture_slot(
            "Metallic Map",
            "metallic_map",
            MaterialData::FLAG_METALLIC_MAP,
            VK_FORMAT_R8G8B8A8_UNORM,
            "Drag & drop metallic texture here. Right-click to clear.");

          draw_texture_slot("AO Map",
                            "ao_map",
                            MaterialData::FLAG_AO_MAP,
                            VK_FORMAT_R8G8B8A8_UNORM,

                            "Drag & drop ambient occlusion texture here. "
                            "Right-click to clear.");

          draw_texture_slot(
            "Emissive Map",
            "emissive_map",
            MaterialData::FLAG_EMISSIVE_MAP,
            VK_FORMAT_R8G8B8A8_UNORM,
            "Drag & drop emissive texture here. Right-click to clear.");

          ImGui::TreePop();
        }

        ImGui::Separator();

        // --- Material Flags Section ---
        if (ImGui::TreeNode("Flags")) {
          bool alpha_test = mat_data.is_alpha_testing();
          if (ImGui::Checkbox("Alpha Testing", &alpha_test)) {
            mat_data.set_alpha_testing(alpha_test);
          }

          bool double_sided = mat_data.flags & MaterialData::FLAG_DOUBLE_SIDED;
          if (ImGui::Checkbox("Double Sided", &double_sided)) {
            mat_data.set_double_sided(double_sided);
          }

          bool emissive = mat_data.is_emissive();
          if (ImGui::Checkbox("Emissive", &emissive)) {
            mat_data.set_emissive(emissive);
          }

          ImGui::Separator();
          ImGui::Text("Texture Flags (Read-only):");
          ImGui::Text("Albedo Texture: %s",
                      mat_data.has_albedo_texture() ? "Yes" : "No");
          ImGui::Text("Normal Map: %s",
                      mat_data.has_normal_map() ? "Yes" : "No");
          ImGui::Text("Roughness Map: %s",
                      mat_data.has_roughness_map() ? "Yes" : "No");
          ImGui::Text("Metallic Map: %s",
                      mat_data.has_metallic_map() ? "Yes" : "No");
          ImGui::Text("AO Map: %s", mat_data.has_ao_map() ? "Yes" : "No");
          ImGui::Text("Emissive Map: %s",
                      mat_data.has_emissive_map() ? "Yes" : "No");

          ImGui::TreePop();
        }

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

    if (auto* light = registry.try_get<Component::PointLight>(entity)) {
      if (ImGui::TreeNode("Point Light")) {
        bool modified = false;
        modified |= draw_vector3_slider("Color", light->color, 0.0f, 10.0f);
        modified |=
          ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 100.0f);
        modified |= ImGui::SliderFloat("Radius", &light->radius, 0.0f, 100.0f);
        modified |= ImGui::Checkbox("Cast Shadows", &light->cast_shadows);

        if (modified) {
          light->dirty = true;
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
      if (!e.has_component<Component::Mesh>() && ImGui::MenuItem("Mesh")) {
        e.add_component<Component::Mesh>();
      }
      if (!e.has_component<Component::PointLight>() &&
          ImGui::MenuItem("Light")) {
        e.add_component<Component::PointLight>();
      }
      if (!e.has_component<Component::Camera>() && ImGui::MenuItem("Camera")) {
        e.add_component<Component::Camera>();
      }
      if (!e.has_component<Component::Material>() &&
          ImGui::MenuItem("Material")) {
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
Scene::draw_entity_hierarchy(entt::entity entity,
                             const std::string& search_term) -> bool
{
  ZoneScopedN("Scene::draw_entity_hierarchy");
  const auto& tag = registry.get<Component::Tag>(entity);
  const auto& hierarchy = registry.get<Component::Hierarchy>(entity);
  std::string name = tag.name;
  std::ranges::transform(name, name.begin(), ::tolower);
  const bool name_matches = name.contains(search_term);

  bool child_matches = false;
  for (entt::entity child : hierarchy.children) {
    if (has_matching_child(child, search_term)) {
      child_matches = true;
      break;
    }
  }

  if (!name_matches && !child_matches)
    return false;

  ImGuiTreeNodeFlags flags =
    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
    (hierarchy.children.empty() ? ImGuiTreeNodeFlags_Leaf : 0) |
    (selected_entity == entity ? ImGuiTreeNodeFlags_Selected : 0);

  const bool node_open =
    ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                      flags,
                      "%s",
                      tag.name.c_str());

  if (ImGui::IsItemClicked())
    selected_entity = entity;

  if (node_open) {
    for (entt::entity child : hierarchy.children) {
      draw_entity_hierarchy(child, search_term);
    }
    ImGui::TreePop();
  }

  return true;
}

auto
Scene::has_matching_child(entt::entity entity, const std::string& search_term)
  -> bool
{
  const auto& tag = registry.get<Component::Tag>(entity);
  const auto& hierarchy = registry.get<Component::Hierarchy>(entity);

  std::string name = tag.name;
  std::ranges::transform(name, name.begin(), ::tolower);

  if (name.contains(search_term))
    return true;

  for (const auto& child : hierarchy.children) {
    if (has_matching_child(child, search_term))
      return true;
  }

  return false;
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

  for (auto&& [entity, tag, hierarchy] :
       registry.view<Component::Tag, Component::Hierarchy>().each()) {

    if (hierarchy.parent == entt::null) {
      draw_entity_hierarchy(entity, search_term);
    }
  }

  ImGui::EndChild();

  ImGui::End();

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
  auto& point_light_system = renderer.get_point_light_system();
  for (const auto view =
         registry.view<Component::PointLight, const Component::Transform>();
       auto&& [entity, light, transform] : view.each()) {
    if (light.dirty) {
      point_light_system.update_light_position(entt::to_integral(entity),
                                               transform.position);
      point_light_system.update_light_component(entt::to_integral(entity),
                                                light);
      light.dirty = false;
    }

    if constexpr (is_debug) {
      auto mesh = Assets::builtin_sphere();
      renderer.submit(
        {
          .mesh = mesh.get(),
          .override_material = point_light_system.get_material(),
        },
        transform.compute(),
        entt::to_integral(entity));
    }
  }

  for (const auto view =
         registry.view<Component::Mesh, const Component::Transform>(
           entt::exclude<Component::PointLight>);
       auto&& [entity, mesh, transform] : view.each()) {
    if (!mesh.mesh.is_valid())
      return;

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

  return event.handled;
}

auto
Scene::selected_is_valid() const -> bool
{
  return registry.valid(selected_entity);
}

auto
Scene::on_resize(const EditorCamera& camera) -> void
{
  if (auto* cam = scene_camera_entity.try_get<Component::Camera>();
      nullptr != cam) {
    const auto& config = camera.get_projection_config();
    cam->fov = config.fov;
    cam->znear = config.znear;
    cam->zfar = config.zfar;
    cam->aspect = config.aspect;
    cam->projection = camera.get_projection();
    cam->inverse_projection = camera.get_inverse_projection();
    cam->view = camera.get_view();
    cam->dirty = true;
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
