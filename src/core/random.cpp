#include "core/random.hpp"
#include <random>

namespace {
thread_local std::mt19937 rng{ std::random_device{}() };
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

}
