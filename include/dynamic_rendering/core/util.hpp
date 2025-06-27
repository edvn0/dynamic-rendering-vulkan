#pragma once

#include <algorithm>
#include <bitset>
#include <string>
#include <string_view>

/// @brief Sometimes, we need to use void* or const void* as a pointer type.
/// We supply these aliases to localise the sonarqube and static code analysis
/// tool errors to one place.
namespace Pointers {
using transparent = void*;
using transparent_const = const void*;
}

struct string_hash
{
  using is_transparent = void;

  [[nodiscard]] auto operator()(std::string_view txt) const noexcept
    -> std::size_t
  {
    return std::hash<std::string_view>{}(txt);
  }

  [[nodiscard]] auto operator()(const std::string& txt) const noexcept
    -> std::size_t
  {
    return std::hash<std::string_view>{}(txt);
  }

  [[nodiscard]] auto operator()(const char* txt) const noexcept -> std::size_t
  {
    return std::hash<std::string_view>{}(txt);
  }
};

using string_hash_set =
  std::unordered_set<std::string, string_hash, std::equal_to<>>;
template<typename T>
using string_hash_map =
  std::unordered_map<std::string, T, string_hash, std::equal_to<>>;

template<typename T, std::predicate<std::string, T> F>
inline auto
erase_from_map_if(string_hash_map<T>& map, F&& functor) -> void
{
  for (auto it = map.begin(); it != map.end();) {
    if (functor(it->first, it->second)) {
      it = map.erase(it);
    } else {
      ++it;
    }
  }
}

/**
 * @brief A utility class to implement the "badge" idiom for restricting access
 * to certain constructors or methods.
 *
 * The Badge class is used to grant specific classes or functions privileged
 * access to otherwise restricted constructors or methods. By making the
 * constructor of Badge private and declaring a specific class or function as a
 * friend, only that class or function can create instances of Badge. This
 * allows for fine-grained control over access to certain parts of the codebase.
 *
 * @tparam T The class or function that is granted privileged access.
 *
 * @note This idiom is particularly useful in scenarios where you want to
 * enforce strict encapsulation and prevent misuse of certain APIs, while still
 * allowing specific trusted entities to bypass those restrictions.
 */
template<typename T>
class Badge
{
  friend T;
  Badge() = default;
};

/**
 * @brief Creates a buffer of bytes sufficient to hold a specified number of
 * objects of type T.
 *
 * This function allocates a contiguous block of memory, represented as an array
 * of `std::byte`, large enough to store `Count` objects of type `T`. The
 * function ensures that the type `T` satisfies the following constraints:
 * - `T` must be a Plain Old Data (POD) type.
 * - `T` must be trivially copyable.
 * - `T` must be a trivial type.
 *
 * @tparam T The type of the objects for which the byte buffer is created.
 * @tparam Count The number of objects of type `T` that the buffer should
 * accommodate.
 *
 * @return A `std::tuple` containing:
 * - A `std::unique_ptr` to the allocated byte buffer.
 * - The total size of the allocated buffer in bytes.
 */
template<typename T, std::size_t Count>
  requires std::is_pod_v<T> && std::is_trivially_copyable_v<T> &&
           std::is_trivial_v<T>
auto
make_bytes() -> std::tuple<std::unique_ptr<std::byte[]>, std::size_t>
{
  return std::make_tuple(std::make_unique<std::byte[]>(sizeof(T) * Count),
                         sizeof(T) * Count);
}

static constexpr auto
any(const auto& needle, auto&&... haystack)
{
  return ((needle == haystack) || ...);
}

static constexpr auto
all(const auto& needle, auto&&... haystack)
{
  return ((needle == haystack) && ...);
}
