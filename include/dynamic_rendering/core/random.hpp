#pragma once

#include <glm/glm.hpp>

namespace VkMaths {
class AABB;
}

namespace Util::Random {

auto
random_float(float min, float max) -> float;

auto
random_colour() -> glm::vec4;

auto
random_single_channel_colour() -> glm::vec4;

auto
random_vec4(float, float) -> glm::vec4;
auto
random_vec3(float, float) -> glm::vec3;
auto
random_vec3(const VkMaths::AABB&) -> glm::vec3;

}