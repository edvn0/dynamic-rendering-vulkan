#include "core/random.hpp"
#include "renderer/mesh.hpp"
#include <random>

namespace {
thread_local std::mt19937 rng{};
}

namespace Util::Random {

auto
random_vec4(float min, float max) -> glm::vec4
{
  std::uniform_real_distribution<float> dist(min, max);
  return {
    dist(rng),
    dist(rng),
    dist(rng),
    dist(rng),
  };
}

auto
random_vec3(float min, float max) -> glm::vec3
{
  std::uniform_real_distribution<float> dist(min, max);
  return {
    dist(rng),
    dist(rng),
    dist(rng),
  };
}

auto
random_vec3(const VkMaths::AABB& aabb) -> glm::vec3
{
  std::uniform_real_distribution<float> x_dist(aabb.min().x, aabb.max().x);
  std::uniform_real_distribution<float> y_dist(aabb.min().y, aabb.max().y);
  std::uniform_real_distribution<float> z_dist(aabb.min().z, aabb.max().z);
  return {
    x_dist(rng),
    y_dist(rng),
    z_dist(rng),
  };
}

auto
random_float(float min, float max) -> float
{
  std::uniform_real_distribution<float> dist(min, max);
  return dist(rng);
}

auto
random_colour() -> glm::vec4
{
  return {
    random_float(0.f, 1.f), random_float(0.f, 1.f), random_float(0.f, 1.f), 1.f
  };
}

auto
random_single_channel_colour() -> glm::vec4
{
  static std::uniform_int_distribution<int> dist(0, 2);

  switch (dist(rng)) {
    case 0:
      return { 1.f, 0.f, 0.f, 1.f };
    case 1:
      return { 0.f, 1.f, 0.f, 1.f };
    case 2:
      return { 0.f, 0.f, 1.f, 1.f };
    default:
      return { 0.f, 0.f, 0.f, 1.f };
  }
}
}
