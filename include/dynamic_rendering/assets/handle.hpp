#pragma once

#include <cstdint>

namespace Assets {

template<typename T>
struct Handle
{
  using id_type = std::uint32_t;
  static constexpr auto invalid = static_cast<std::uint32_t>(~0);
  id_type id = invalid;

  constexpr Handle() = default;
  constexpr explicit Handle(const id_type id)
    : id(id)
  {
  }

  auto reset() -> void { id = invalid; }

  [[nodiscard]]
  auto is_valid() const -> bool
  {
    return id != invalid;
  }

  auto get() -> T*;
  auto get() const -> const T*;

  auto operator<=>(const Handle& other) const = default;
};

}
