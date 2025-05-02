#pragma once

#include <concepts>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace detail {

template<typename T>
concept has_xy_fields = requires(const T& t) {
  { t.x };
  { t.y };
};

template<typename T>
concept has_width_height_fields = requires(const T& t) {
  { t.width };
  { t.height };
};

template<typename T>
concept has_tuple_like_integrals = requires(const std::remove_cvref_t<T>& t) {
  { std::get<0>(t) };
  { std::get<1>(t) };
};

} // namespace detail

template<typename T>
concept ExtentConstructible =
  detail::has_xy_fields<T> || detail::has_width_height_fields<T> ||
  detail::has_tuple_like_integrals<T>;

struct Extent2D
{
  std::uint32_t width{ 0 };
  std::uint32_t height{ 0 };

  constexpr Extent2D() = default;

  template<std::integral T>
  constexpr Extent2D(T w, T h)
    : width(static_cast<std::uint32_t>(w))
    , height(static_cast<std::uint32_t>(h))
  {
  }

  template<ExtentConstructible T>
  constexpr Extent2D(const T& t)
  {
    if constexpr (detail::has_xy_fields<T>) {
      width = static_cast<std::uint32_t>(t.x);
      height = static_cast<std::uint32_t>(t.y);
    } else if constexpr (detail::has_width_height_fields<T>) {
      width = static_cast<std::uint32_t>(t.width);
      height = static_cast<std::uint32_t>(t.height);
    } else {
      width = static_cast<std::uint32_t>(std::get<0>(t));
      height = static_cast<std::uint32_t>(std::get<1>(t));
    }
  }

  auto operator<=>(const Extent2D&) const = default;

  template<std::integral T>
  constexpr auto as() const -> Extent2D
  {
    return Extent2D{ static_cast<T>(width), static_cast<T>(height) };
  }
};
