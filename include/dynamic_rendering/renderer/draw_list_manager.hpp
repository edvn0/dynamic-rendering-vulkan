#pragma once

#include "core/gpu_buffer.hpp"
#include "renderer/draw_command.hpp"
#include "renderer/frustum.hpp"

#include <atomic>
#include <cstdint>
#include <execution>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <BS_thread_pool.hpp>

class DrawListManager
{
public:
  static auto should_perform_culling(const DrawCommandMap& draw_map,
                                     std::size_t threshold = 500) -> bool;

  static auto flatten_draw_commands(const DrawCommandMap& draw_map,
                                    GPUBuffer& buffer,
                                    std::uint32_t& instance_count) -> DrawList;

  static auto cull_and_flatten_draw_commands(const DrawCommandMap& draw_map,
                                             GPUBuffer& buffer,
                                             const Frustum& frustum,
                                             std::uint32_t& instance_count,
                                             BS::priority_thread_pool& pool)
    -> DrawList;
};

// Wrapper around a 2 func submit that returns a tuple of the two results. The
// "types" of the two functions are the same.
template<typename... Fs>
auto
submit_tuple_and_wait(BS::priority_thread_pool& pool, Fs&&... funcs)
{
  auto futures = std::make_tuple(pool.submit_task(std::forward<Fs>(funcs))...);
  return std::apply(
    [](auto&... futs) { return std::make_tuple(futs.get()...); }, futures);
}