#include "renderer/draw_list_manager.hpp"

#include "core/allocator.hpp"
#include "renderer/draw_command.hpp"

// Return true if total instance count in the draw map exceeds the given
// threshold
auto
DrawListManager::should_perform_culling(const DrawCommandMap& draw_map,
                                        std::size_t threshold) -> bool
{
  std::size_t total = 0;
  for (const auto& [_, instances] : draw_map)
    total += instances.size();
  return total >= threshold;
}

auto
DrawListManager::flatten_draw_commands(const DrawCommandMap& draw_map,
                                       GPUBuffer& buffer,
                                       std::uint32_t& instance_count)
  -> DrawList
{
  using DrawCommandEntry = std::pair<DrawCommand, std::vector<InstanceData>>;

  DrawList flat_list;
  instance_count = 0;

  static std::size_t reserved_size = 1024;
  std::vector<InstanceData> all_instances;
  all_instances.reserve(reserved_size);

  std::vector<DrawCommandEntry> sorted;
  sorted.reserve(draw_map.size());

  for (const auto& [cmd, instances] : draw_map)
    sorted.emplace_back(cmd, instances);

  std::ranges::sort(sorted, [](const auto& a, const auto& b) {
    return a.first.override_material < b.first.override_material;
  });

  for (const auto& [cmd, instances] : sorted) {
    if (instances.empty())
      continue;
    const auto offset = static_cast<std::uint32_t>(all_instances.size());
    all_instances.insert(
      all_instances.end(), instances.begin(), instances.end());
    flat_list.emplace_back(
      cmd, offset, static_cast<std::uint32_t>(instances.size()));
  }

  instance_count = static_cast<std::uint32_t>(all_instances.size());
  if (instance_count > 0)
    buffer.upload(std::span(all_instances));

  reserved_size = std::max(reserved_size, all_instances.size());

  return flat_list;
}

// Performs frustum culling and returns a flattened draw list of visible items
auto
DrawListManager::cull_and_flatten_draw_commands(const DrawCommandMap& draw_map,
                                                GPUBuffer& buffer,
                                                const Frustum& frustum,
                                                std::uint32_t& instance_count,
                                                BS::priority_thread_pool& pool)
  -> DrawList
{
  std::size_t estimated_total = 0;
  for (const auto& [_, instances] : draw_map)
    estimated_total += instances.size();

  std::vector<DrawInstanceSubmit> filtered_instances(estimated_total);
  std::atomic<std::size_t> filtered_count{ 0 };

  std::vector<std::pair<const DrawCommand*, const std::vector<InstanceData>*>>
    jobs;
  jobs.reserve(draw_map.size());
  for (const auto& [cmd, instances] : draw_map)
    jobs.emplace_back(&cmd, &instances);

  // Parallel frustum culling
  auto fut = pool.submit_loop(0, jobs.size(), [&](std::size_t i) {
    const auto& [cmd, instances] = jobs[i];
    for (const auto& instance : *instances) {
      const auto center = glm::vec3(instance.transform[3]);
      const float radius = 1.0f * glm::length(glm::vec3(instance.transform[0]));
      if (frustum.intersects(center, radius)) {
        const std::size_t index =
          filtered_count.fetch_add(1, std::memory_order_relaxed);
        filtered_instances[index] = { cmd, instance };
      }
    }
  });
  fut.wait();

  // Shrink to actual number of visible instances
  filtered_instances.resize(filtered_count);

  // Output total visible instance count for this frame
  instance_count = static_cast<std::uint32_t>(filtered_instances.size());

  // Early out if no instances are visible
  if (filtered_instances.empty())
    return {};

  // Copy visible instance data to a contiguous buffer for upload
  static std::size_t reserved_instance_data = 1024;
  std::vector<InstanceData> instance_data;
  instance_data.reserve(
    std::max(reserved_instance_data, filtered_instances.size()));
  instance_data.resize(filtered_instances.size());

  std::transform(std::execution::par_unseq,
                 filtered_instances.begin(),
                 filtered_instances.end(),
                 instance_data.begin(),
                 [](const DrawInstanceSubmit& s) { return s.data; });

  buffer.upload(std::span(instance_data));
  reserved_instance_data =
    std::max(reserved_instance_data, instance_data.size());

  // Build flattened draw command map that records instance offset and count per
  // draw command
  std::unordered_map<DrawCommand,
                     std::tuple<std::uint32_t, std::uint32_t>,
                     DrawCommandHasher>
    draw_map_flat;
  for (std::size_t i = 0; i < filtered_instances.size(); ++i) {
    const auto* cmd = filtered_instances[i].cmd;
    auto& [start, count] = draw_map_flat[*cmd];
    if (count == 0)
      start = static_cast<std::uint32_t>(i);
    ++count;
  }

  // Convert map to a linear draw list for submission
  DrawList result;
  result.reserve(draw_map_flat.size());
  for (const auto& [cmd, range] : draw_map_flat)
    result.push_back({ cmd, std::get<0>(range), std::get<1>(range) });

  return result;
}
