#pragma once

#include "core/config.hpp"
#include "dynamic_rendering/core/forward.hpp"
#include "dynamic_rendering/core/gpu_buffer.hpp"
#include "scene/components.hpp"

#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <vector>

class PointLightSystem
{
public:
  static constexpr std::size_t max_point_lights = 4096;
  static constexpr std::uint32_t descriptor_set = 0;
  static constexpr std::uint32_t descriptor_binding = 4;

  explicit PointLightSystem(const Device&);
  ~PointLightSystem() = default;

  [[nodiscard]] auto get_ssbo(const Badge<Renderer>) const -> auto*
  {
    return point_lights_gpu.get();
  }
  [[nodiscard]] auto get_light_count() const { return active_light_count; }

  auto upload_to_gpu(std::uint32_t) -> void;

  auto add_light(std::uint32_t entity_id,
                 const glm::vec3&,
                 const Component::PointLight&) -> std::uint32_t;
  auto update_light_position(std::uint32_t entity_id, const glm::vec3&) -> void;
  auto update_light_component(std::uint32_t entity_id,
                              const Component::PointLight&) -> void;
  auto remove_light(std::uint32_t entity_id) -> void;
  [[nodiscard]] auto get_material() const -> Assets::Handle<Material>
  {
    return point_light_material;
  }

private:
  const Device* device{ nullptr };
  std::unique_ptr<GPUBuffer> point_lights_gpu{ nullptr };

  Assets::Handle<Material> point_light_material{};
  std::vector<std::uint32_t> light_index_to_entity{};
  std::vector<std::uint32_t> entity_to_light_index{};
  std::vector<glm::vec3> positions{};
  std::vector<glm::vec3> colors{};
  std::vector<float> intensities{};
  std::vector<float> radii{};
  std::vector<bool> cast_shadows{};
  std::vector<bool> dirty_flags{};

  std::uint32_t active_light_count{ 0 };
  std::bitset<frames_in_flight> gpu_buffer_dirty{ false };
  auto set() -> void { gpu_buffer_dirty.set(); }
  auto reset(std::size_t i) -> void { gpu_buffer_dirty.reset(i); }
  auto set_all() -> void
  {
    for (std::size_t i = 0; i < frames_in_flight; ++i) {
      gpu_buffer_dirty.set(i);
    }
  }
  auto reset_all() -> void
  {
    for (std::size_t i = 0; i < frames_in_flight; ++i) {
      gpu_buffer_dirty.reset(i);
    }
  }
};
