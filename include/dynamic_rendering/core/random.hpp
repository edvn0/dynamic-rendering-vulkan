#pragma once

#include <glm/glm.hpp>

namespace Utils::Random {
auto
random_float(float min, float max) -> float;
auto
random_colour() -> glm::vec4;
}