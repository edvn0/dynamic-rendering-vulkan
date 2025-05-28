#pragma once

#include <dynamic_rendering/dynamic_rendering.hpp>

class AppLayer final
  : public ILayer
  , public DynamicRendering::IRayPickListener
  , public DynamicRendering::ViewportBoundsListener
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
  auto on_initialise(const InitialisationParameters&) -> void override;

  auto on_viewport_bounds_changed(
    const DynamicRendering::ViewportBounds& new_viewport_bounds)
    -> void override
  {
    active_scene->update_viewport_bounds(new_viewport_bounds);
  }
  auto on_ray_pick(const glm::vec3& origin, const glm::vec3& direction)
    -> void override;

private:
  const DynamicRendering::App* app{ nullptr };
  BS::priority_thread_pool* thread_pool{ nullptr };
  BlueprintRegistry* blueprint_registry{ nullptr };

  void generate_scene();

  glm::vec2 bounds{};

  std::shared_ptr<Scene> active_scene;

  float rotation_speed{ 3.0F };
  LightEnvironment light_environment;

  std::vector<glm::mat4> transforms;
  std::vector<Material> materials;
};
