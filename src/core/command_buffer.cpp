#include "core/command_buffer.hpp"

#include "core/device.hpp"

auto
CommandBuffer::create(const Device& device, VkCommandPoolCreateFlags pool_flags)
  -> std::unique_ptr<CommandBuffer>
{
  return std::make_unique<CommandBuffer>(
    device, device.graphics_queue(), CommandBufferType::Graphics, pool_flags);
}

CommandBuffer::CommandBuffer(const Device& dev,
                             const VkQueue q,
                             const CommandBufferType command_buffer_type,
                             const VkCommandPoolCreateFlags pool_flags)
  : timestamp_period(dev.get_timestamp_period())
  , execution_queue(q)
  , device(&dev)
  , command_buffer_type(command_buffer_type)
{
  create_command_pool(pool_flags);
  allocate_command_buffers();
  create_query_pools();
  create_fences();
}

CommandBuffer::~CommandBuffer()
{
  for (auto& pool : query_pools) {
    vkDestroyQueryPool(device->get_device(), pool, nullptr);
  }

  vkDestroyCommandPool(device->get_device(), command_pool, nullptr);
  for (auto& fence : fences) {
    vkDestroyFence(device->get_device(), fence, nullptr);
  }
}

auto
CommandBuffer::read_timestamps(const uint32_t frame_index,
                               const uint32_t first_query,
                               const uint32_t query_count) const
  -> std::optional<std::vector<double>>
{
  std::vector<std::uint64_t> timestamps(query_count);
  const auto result =
    vkGetQueryPoolResults(device->get_device(),
                          query_pools[frame_index],
                          first_query,
                          query_count,
                          sizeof(std::uint64_t) * query_count,
                          timestamps.data(),
                          sizeof(std::uint64_t),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

  if (result != VK_SUCCESS)
    return std::nullopt;

  std::vector<double> deltas;
  deltas.reserve(query_count - 1);

  for (uint32_t i = 1; i < query_count; ++i) {
    auto delta = (timestamps[i] - timestamps[i - 1]) * timestamp_period * 1e-6;
    deltas.push_back(delta);
  }

  return deltas;
}

void
CommandBuffer::create_command_pool(const VkCommandPoolCreateFlags flags)
{
  VkCommandPoolCreateInfo info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = flags,
    .queueFamilyIndex = device->get_queue_family_index(execution_queue),
  };
  vkCreateCommandPool(device->get_device(), &info, nullptr, &command_pool);
}

void
CommandBuffer::create_fences()
{
  VkFenceCreateInfo info{
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for (auto& fence : fences) {
    vkCreateFence(device->get_device(), &info, nullptr, &fence);
  }
}

void
CommandBuffer::allocate_command_buffers()
{
  VkCommandBufferAllocateInfo info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = static_cast<uint32_t>(command_buffers.size()),
  };
  vkAllocateCommandBuffers(device->get_device(), &info, command_buffers.data());
}

void
CommandBuffer::create_query_pools()
{
  for (auto& pool : query_pools) {
    VkQueryPoolCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = 64,
      .pipelineStatistics = 0,
    };
    vkCreateQueryPool(device->get_device(), &info, nullptr, &pool);
  }
}

void
CommandBuffer::begin(const uint32_t frame_index,
                     const VkCommandBufferUsageFlags usage) const
{
  VkCommandBufferBeginInfo info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = usage,
    .pInheritanceInfo = nullptr,
  };
  vkBeginCommandBuffer(command_buffers[frame_index], &info);
}

void
CommandBuffer::end(const uint32_t frame_index) const
{
  vkEndCommandBuffer(command_buffers[frame_index]);
}

void
CommandBuffer::submit(const uint32_t frame_index,
                      const VkSemaphore wait_semaphore,
                      const VkSemaphore signal_semaphore,
                      const VkFence fence) const
{
  submit(frame_index,
         std::span{ &wait_semaphore, 1 },
         std::span{ &signal_semaphore, 1 },
         fence);
}

template<typename T>
constexpr bool
is_valid(const std::span<const T> span)
{
  return !span.empty() &&
         std::ranges::all_of(span, [](auto ptr) { return ptr != nullptr; });
}

void
CommandBuffer::submit(const uint32_t frame_index,
                      const std::span<const VkSemaphore> wait_semaphores,
                      const std::span<const VkSemaphore> signal_semaphores,
                      const VkFence fence) const
{
  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  const VkPipelineStageFlags stage =
    execution_queue == device->compute_queue()
      ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
      : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  if (is_valid(wait_semaphores)) {
    submit_info.waitSemaphoreCount =
      static_cast<std::uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = &stage;
  }

  if (is_valid(signal_semaphores)) {
    submit_info.signalSemaphoreCount =
      static_cast<std::uint32_t>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.data();
  }

  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers[frame_index];

  vkQueueSubmit(execution_queue,
                1,
                &submit_info,
                VK_NULL_HANDLE != fence ? fence : fences[frame_index]);
}

void
CommandBuffer::submit_and_end(const uint32_t frame_index,
                              const VkSemaphore wait_semaphore,
                              const VkSemaphore signal_semaphore,
                              const VkFence fence) const
{
  end(frame_index);
  submit(frame_index,
         std::span{ &wait_semaphore, 1 },
         std::span{ &signal_semaphore, 1 },
         fence);
}

void
CommandBuffer::submit_and_end(
  const uint32_t frame_index,
  const std::span<const VkSemaphore> wait_semaphores,
  const std::span<const VkSemaphore> signal_semaphores,
  const VkFence fence) const
{
  end(frame_index);
  submit(frame_index, wait_semaphores, signal_semaphores, fence);
}

void
CommandBuffer::reset_query_pool(const uint32_t frame_index) const
{
  vkCmdResetQueryPool(
    command_buffers[frame_index], query_pools[frame_index], 0, 64);
}

VkCommandBuffer
CommandBuffer::get(const uint32_t frame_index) const
{
  return command_buffers[frame_index];
}

VkQueryPool
CommandBuffer::get_query_pool(const uint32_t frame_index) const
{
  return query_pools[frame_index];
}

void
CommandBuffer::reset_pool() const
{
  vkResetCommandPool(device->get_device(), command_pool, 0);
}

VkFence
CommandBuffer::get_fence(const uint32_t frame_index) const
{
  return fences[frame_index];
}

void
CommandBuffer::wait_for_fence(const uint32_t frame_index) const
{
  vkWaitForFences(
    device->get_device(), 1, &fences[frame_index], VK_TRUE, UINT64_MAX);
}

void
CommandBuffer::reset_fence(const uint32_t frame_index) const
{
  vkResetFences(device->get_device(), 1, &fences[frame_index]);
}
auto
CommandBuffer::begin_frame(const std::uint32_t frame_index) const -> void
{
  wait_for_fence(frame_index);
  reset_fence(frame_index);
  vkResetCommandBuffer(get(frame_index), 0);
  begin(frame_index);
  reset_query_pool(frame_index);
  timer_sections[frame_index].clear();
  next_query_index[frame_index] = 0;
}

auto
CommandBuffer::begin_frame_persist_query_pools(
  const std::uint32_t frame_index) const -> void
{
  wait_for_fence(frame_index);
  reset_fence(frame_index);
  vkResetCommandBuffer(get(frame_index), 0);
  begin(frame_index);
}

auto
CommandBuffer::resolve_timers(const uint32_t frame_index) const
  -> std::vector<TimedSection>
{
  constexpr uint32_t delay_frames = 2;
  uint32_t resolve_index =
    (frame_index + frames_in_flight - delay_frames) % frames_in_flight;

  std::vector<TimedSection> resolved;
  if (timer_sections[resolve_index].empty())
    return resolved;

  uint32_t max_query = next_query_index[resolve_index];
  std::vector<uint64_t> raw(max_query);

  auto result =
    vkGetQueryPoolResults(device->get_device(),
                          query_pools[resolve_index],
                          0,
                          max_query,
                          sizeof(uint64_t) * raw.size(),
                          raw.data(),
                          sizeof(uint64_t),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

  if (result != VK_SUCCESS)
    return resolved;

  for (auto& [_, section] : timer_sections[resolve_index]) {
    if (section.end_query > section.begin_query) {
      auto delta = (raw[section.end_query] - raw[section.begin_query]) *
                   timestamp_period * 1e-6;
      section.duration_ms = delta;
      resolved.push_back(section);
    }
  }

  return resolved;
}

void
CommandBuffer::write_timestamp(const uint32_t frame_index,
                               const uint32_t query_index) const
{
  VkPipelineStageFlagBits stage =
    command_buffer_type == CommandBufferType::Compute
      ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
      : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

  vkCmdWriteTimestamp(
    command_buffers[frame_index], stage, query_pools[frame_index], query_index);
}

void
CommandBuffer::begin_timer(const uint32_t frame_index,
                           const std::string_view name) const
{
  auto& index = next_query_index[frame_index];
  auto begin_idx = index++;
  write_timestamp(frame_index, begin_idx);

  auto key = std::string(name);
  auto& section = timer_sections[frame_index][key];
  section.name = key;
  section.begin_query = begin_idx;
}

void
CommandBuffer::end_timer(const uint32_t frame_index,
                         const std::string_view name) const
{
  auto& index = next_query_index[frame_index];
  auto end_idx = index++;
  write_timestamp(frame_index, end_idx);

  if (auto it = timer_sections[frame_index].find(name);
      it != timer_sections[frame_index].end()) {
    it->second.end_query = end_idx;
  }
}
