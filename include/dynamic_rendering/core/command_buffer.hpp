#pragma once

#include "core/config.hpp"
#include "core/forward.hpp"
#include "core/util.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct TimedSection
{
  std::string name;
  std::uint32_t begin_query{ 0 };
  std::uint32_t end_query{ 0 };
  double duration_ms{ 0.0 };

  [[nodiscard]] auto duration_ns() const -> std::uint64_t
  {
    return static_cast<std::uint64_t>(duration_ms * 1e6);
  }
};

enum class CommandBufferType : std::uint8_t
{
  Graphics,
  Compute,
};

class CommandBuffer
{
public:
  CommandBuffer(const Device&,
                VkQueue,
                CommandBufferType,
                VkCommandPoolCreateFlags = 0);
  ~CommandBuffer();

  void begin(std::uint32_t frame_index,
             VkCommandBufferUsageFlags usage =
               VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) const;
  void end(std::uint32_t frame_index) const;
  void submit(std::uint32_t frame_index,
              VkSemaphore wait_semaphore = VK_NULL_HANDLE,
              VkSemaphore signal_semaphore = VK_NULL_HANDLE,
              VkFence fence = VK_NULL_HANDLE) const;
  void submit(std::uint32_t frame_index,
              std::span<const VkSemaphore> wait_semaphores,
              std::span<const VkSemaphore> signal_semaphores,
              VkFence fence = VK_NULL_HANDLE) const;
  void submit_and_end(std::uint32_t frame_index,
                      VkSemaphore wait_semaphore = VK_NULL_HANDLE,
                      VkSemaphore signal_semaphore = VK_NULL_HANDLE,
                      VkFence fence = VK_NULL_HANDLE) const;
  void submit_and_end(std::uint32_t frame_index,
                      std::span<const VkSemaphore> wait_semaphores,
                      std::span<const VkSemaphore> signal_semaphore,
                      VkFence fence = VK_NULL_HANDLE) const;

  void reset_query_pool(std::uint32_t frame_index) const;

  VkCommandBuffer get(std::uint32_t frame_index) const;
  VkQueryPool get_query_pool(std::uint32_t frame_index) const;
  auto reset_pool() const -> void;
  VkFence get_fence(std::uint32_t frame_index) const;
  auto wait_for_fence(std::uint32_t frame_index) const -> void;
  auto reset_fence(std::uint32_t frame_index) const -> void;
  auto begin_frame(std::uint32_t frame_index) const -> void;
  auto begin_frame_persist_query_pools(std::uint32_t frame_index) const -> void;

  auto read_timestamps(std::uint32_t frame_index,
                       std::uint32_t first_query,
                       std::uint32_t query_count) const
    -> std::optional<std::vector<double>>;

  void begin_timer(std::uint32_t, std::string_view name);
  void end_timer(std::uint32_t, std::string_view name);
  auto resolve_timers(std::uint32_t) const -> std::vector<TimedSection>;

  static auto create(const Device& device,
                     VkCommandPoolCreateFlags pool_flags = 0)
    -> std::unique_ptr<CommandBuffer>;

private:
  using TimedSectionMap =
    std::unordered_map<std::string, TimedSection, string_hash, std::equal_to<>>;
  mutable frame_array<TimedSectionMap> timer_sections{};
  mutable frame_array<uint32_t> next_query_index{};

  double timestamp_period{ 0.0 };
  VkCommandPool command_pool{ VK_NULL_HANDLE };
  frame_array<VkCommandBuffer> command_buffers{};
  frame_array<VkQueryPool> query_pools{};
  frame_array<VkFence> fences{};
  VkQueue execution_queue{ VK_NULL_HANDLE };
  const Device* device{ nullptr };
  CommandBufferType command_buffer_type;

  auto create_command_pool(VkCommandPoolCreateFlags flags) -> void;
  auto allocate_command_buffers() -> void;
  auto create_query_pools() -> void;
  auto create_fences() -> void;

  auto write_timestamp(std::uint32_t, std::uint32_t) const -> void;
};
