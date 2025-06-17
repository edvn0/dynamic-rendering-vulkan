#include "core/random.hpp"
#include <random>

namespace {
thread_local std::mt19937 rng{};
}

namespace Utils::Random {

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
