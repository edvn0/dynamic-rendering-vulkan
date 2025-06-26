#include "renderer/point_light_system.hpp"

#include "assets/manager.hpp"
#include "core/allocator.hpp"
#include "core/config.hpp"
#include "core/device.hpp"
#include "renderer/point_light.hpp"
#include "scene/components.hpp"

struct alignas(16) PointLightSSBO
{
  std::uint32_t light_count;
  std::uint32_t _pad0, _pad1, _pad2;
  GPUPointLight lights[PointLightSystem::max_point_lights];
};

static constexpr auto size_buffer = sizeof(PointLightSSBO);

PointLightSystem::PointLightSystem(const Device& d)
  : device(&d)
{
  point_light_material =
    Assets::Manager::the().load<::Material>("main_geometry");

  static constexpr auto size = frames_in_flight * size_buffer;
  point_lights_gpu =
    GPUBuffer::zero_initialise(*device,
                               size,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               true,
                               "point_lights_ssbo");

  light_index_to_entity.reserve(max_point_lights);
  entity_to_light_index.reserve(max_point_lights);
  positions.reserve(max_point_lights);
  colors.reserve(max_point_lights);
  intensities.reserve(max_point_lights);
  radii.reserve(max_point_lights);
  cast_shadows.reserve(max_point_lights);
  dirty_flags.reserve(max_point_lights);
}

auto
PointLightSystem::add_light(const std::uint32_t entity_id,
                            const glm::vec3& position,
                            const Component::PointLight& component)
  -> std::uint32_t
{
  if (active_light_count >= max_point_lights) {
    return UINT32_MAX;
  }

  const auto light_index = active_light_count++;

  if (light_index >= positions.size()) {
    positions.resize(light_index + 1);
    colors.resize(light_index + 1);
    intensities.resize(light_index + 1);
    radii.resize(light_index + 1);
    cast_shadows.resize(light_index + 1);
    dirty_flags.resize(light_index + 1);
    light_index_to_entity.resize(light_index + 1);
  }

  positions[light_index] = position;
  colors[light_index] = component.color;
  intensities[light_index] = component.intensity;
  radii[light_index] = component.radius;
  cast_shadows[light_index] = component.cast_shadows;
  dirty_flags[light_index] = true;
  light_index_to_entity[light_index] = entity_id;

  if (entity_id >= entity_to_light_index.size()) {
    entity_to_light_index.resize(entity_id + 1, UINT32_MAX);
  }
  entity_to_light_index[entity_id] = light_index;

  set_all();
  return light_index;
}

auto
PointLightSystem::update_light_position(const std::uint32_t entity_id,
                                        const glm::vec3& new_position) -> void
{
  if (entity_id >= entity_to_light_index.size())
    return;

  const auto light_index = entity_to_light_index[entity_id];
  if (light_index == UINT32_MAX || light_index >= active_light_count)
    return;

  positions[light_index] = new_position;
  dirty_flags[light_index] = true;
  set_all();
}

auto
PointLightSystem::update_light_component(const std::uint32_t entity_id,
                                         const Component::PointLight& component)
  -> void
{
  if (entity_id >= entity_to_light_index.size())
    return;

  const auto light_index = entity_to_light_index[entity_id];
  if (light_index == UINT32_MAX || light_index >= active_light_count)
    return;

  colors[light_index] = component.color;
  intensities[light_index] = component.intensity;
  radii[light_index] = component.radius;
  cast_shadows[light_index] = component.cast_shadows;
  dirty_flags[light_index] = true;
  set_all();
}

/// @brief Remove a light
/// @param entity_id Entity identifier
auto
PointLightSystem::remove_light(std::uint32_t entity_id) -> void
{
  if (entity_id >= entity_to_light_index.size())
    return;

  const auto light_index = entity_to_light_index[entity_id];
  if (light_index == UINT32_MAX || light_index >= active_light_count)
    return;

  // Swap with last element to maintain dense array
  if (const auto last_index = active_light_count - 1;
      light_index != last_index) {
    positions[light_index] = positions[last_index];
    colors[light_index] = colors[last_index];
    intensities[light_index] = intensities[last_index];
    radii[light_index] = radii[last_index];
    cast_shadows[light_index] = cast_shadows[last_index];
    dirty_flags[light_index] = dirty_flags[last_index];

    // Update mapping for swapped element
    const auto moved_entity = light_index_to_entity[last_index];
    light_index_to_entity[light_index] = moved_entity;
    entity_to_light_index[moved_entity] = light_index;
  }

  // Clear mapping for removed entity
  entity_to_light_index[entity_id] = UINT32_MAX;
  --active_light_count;
  set_all();
}

auto
PointLightSystem::upload_to_gpu(const std::uint32_t frame_index) -> void
{
  if (!gpu_buffer_dirty[frame_index])
    return;

  const auto offset = sizeof(PointLightSSBO) * frame_index;

  void* mapped_data = nullptr;
  if (vmaMapMemory(device->get_allocator().get(),
                   point_lights_gpu->get_allocation(),
                   &mapped_data) != VK_SUCCESS) {
    return;
  }

  auto* ssbo = reinterpret_cast<PointLightSSBO*>(
    static_cast<std::uint8_t*>(mapped_data) + offset);

  ssbo->light_count = active_light_count;

  for (std::size_t i = 0; i < active_light_count; ++i) {
    ssbo->lights[i].position = positions[i];
    ssbo->lights[i].radius = radii[i];
    ssbo->lights[i].color = colors[i];
    ssbo->lights[i].intensity = intensities[i];
  }

  vmaUnmapMemory(device->get_allocator().get(),
                 point_lights_gpu->get_allocation());

  reset(frame_index);
}
