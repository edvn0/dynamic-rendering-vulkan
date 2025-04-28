#pragma once

#include <array>
#include <cstdint>

static constexpr std::uint32_t image_count = 3;

template<class T>
using frame_array = std::array<T, image_count>;
