#pragma once

#include <array>
#include <cstdint>

static constexpr std::uint32_t max_swapchain_image_count = 3;
static constexpr std::uint32_t frames_in_flight = 3;

template<class T>
using frame_array = std::array<T, frames_in_flight>;

#ifdef IS_DEBUG
static constexpr auto is_debug = true;
#else
static constexpr auto is_debug = false;
#endif