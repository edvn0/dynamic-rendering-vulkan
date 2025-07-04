#pragma once

#include "pointer.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

namespace Assets::detail {

template<typename T>
class TrackingAllocator
{
public:
  using value_type = T;

  TrackingAllocator() = default;

  template<typename U>
  TrackingAllocator(const TrackingAllocator<U>&) noexcept
  {
  }

  [[nodiscard]] auto allocate(std::size_t n) -> T*
  {
    auto bytes = n * sizeof(T);
    total_tracked_bytes += bytes;
    Logger::log_trace("Allocating {} bytes for {}", bytes, typeid(T).name());
    return current_allocator.allocate(n);
  }

  void deallocate(T* p, std::size_t n) noexcept
  {
    auto bytes = n * sizeof(T);
    total_tracked_bytes -= bytes;
    Logger::log_trace("Deallocating {} bytes for {}", bytes, typeid(T).name());
    current_allocator.deallocate(p, n);
  }

  template<typename U>
  struct rebind
  {
    using other = TrackingAllocator<U>;
  };

  static inline std::atomic_ullong total_tracked_bytes = 0;

private:
  static inline std::allocator<T> current_allocator{};
};

template<typename T, typename U>
inline bool
operator==(const TrackingAllocator<T>&, const TrackingAllocator<U>&) noexcept
{
  return true;
}

template<typename T, typename U>
inline bool
operator!=(const TrackingAllocator<T>&, const TrackingAllocator<U>&) noexcept
{
  return false;
}

} // namespace Assets::detail

namespace Assets {
template<typename T, typename... Args>
auto
make_tracked(Args&&... args) -> Assets::Pointer<T>
{
  static detail::TrackingAllocator<T> allocator;

  T* raw = allocator.allocate(1);
  try {
    std::construct_at(raw, std::forward<Args>(args)...);
  } catch (...) {
    allocator.deallocate(raw, 1);
    throw;
  }

  auto deleter = [](T* ptr) {
    std::destroy_at(ptr);
    allocator.deallocate(ptr, 1);
  };

  return Assets::Pointer<T>(raw, deleter);
}

template<typename Base, typename Derived, typename... Args>
auto
make_tracked_as(Args&&... args) -> Assets::Pointer<Base>
{
  static detail::TrackingAllocator<Derived> allocator;

  Derived* raw = allocator.allocate(1);
  try {
    std::construct_at(raw, std::forward<Args>(args)...);
  } catch (...) {
    allocator.deallocate(raw, 1);
    throw;
  }

  auto deleter = [](Base* base_ptr) {
    auto derived_ptr = static_cast<Derived*>(base_ptr);
    std::destroy_at(derived_ptr);
    allocator.deallocate(derived_ptr, 1);
  };

  return Assets::Pointer<Base>(raw, deleter);
}

}