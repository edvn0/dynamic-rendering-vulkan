#pragma once

#include <dynamic_rendering/dynamic_rendering.hpp>

struct FrametimeSmoother;
struct FrameTimePlotter;

class AppLayer final : public ILayer
{
public:
  explicit AppLayer(const Device&, Renderer&, BS::priority_thread_pool*);
  ~AppLayer() override;

  auto on_destroy() -> void override;
  auto on_event(Event& event) -> bool override;
  auto on_interface() -> void override;
  auto on_update(double ts) -> void override;
  auto on_render(Renderer& renderer) -> void override;
  auto on_resize(std::uint32_t w, std::uint32_t h) -> void override;
  auto on_initialise(const InitialisationParameters&) -> void override;
  auto get_camera_matrices(CameraMatrices&) const -> bool override;

private:
  Renderer* renderer{ nullptr };
  BS::priority_thread_pool* thread_pool{ nullptr };

  void generate_scene(PointLightSystem&);
  DynamicRendering::ViewportBounds viewport_bounds;
  glm::vec2 bounds{};
  std::unique_ptr<EditorCamera> camera;

  std::shared_ptr<Scene> active_scene;

  float rotation_speed{ 3.0F };
  LightEnvironment light_environment;

  std::vector<glm::mat4> transforms;
  std::vector<Material> materials;

  std::unique_ptr<FrametimeSmoother> smoother;
  std::unique_ptr<FrameTimePlotter> plotter;

  auto on_ray_pick(const glm::vec3& origin, const glm::vec3& direction) -> void;
};
