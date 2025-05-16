#pragma once

#include <dynamic_rendering/dynamic_rendering.hpp>

class AppLayer : public ILayer
{
public:
  explicit AppLayer(const Device&,
                    BS::priority_thread_pool*,
                    BlueprintRegistry*);
  ~AppLayer() override;

  auto on_destroy() -> void override;
  auto on_event(Event& event) -> bool override;
  auto on_interface() -> void override;
  auto on_update(double ts) -> void override;
  auto on_render(Renderer& renderer) -> void override;
  auto on_resize(std::uint32_t w, std::uint32_t h) -> void override;

private:
  BS::priority_thread_pool* thread_pool{ nullptr };
  BlueprintRegistry* blueprint_registry{ nullptr };

  void generate_scene();

  float rotation_speed = 90.f;
  glm::vec2 bounds{};

  std::unique_ptr<Mesh> mesh{ std::make_unique<Mesh>() };
  std::unique_ptr<Mesh> tokyo_mesh{ std::make_unique<Mesh>() };
  std::unique_ptr<Mesh> hunter_mesh{ std::make_unique<Mesh>() };
  std::unique_ptr<Mesh> armour_mesh{ std::make_unique<Mesh>() };

  glm::vec3 light_position{ 28.f, 23.f, 1.f };
  glm::vec4 light_color{ 1.F, 0.5F, 0.F, 1.F };

  std::vector<glm::mat4> transforms;
  std::vector<Material> materials;
};
