#pragma once

#include <filesystem>

/// @brief This should return the base path of the application, which is the
/// path that contains an assets folder.
/// @return The base path of the application.
auto
base_path() -> const std::filesystem::path&;

auto
assets_path() -> const std::filesystem::path&;

/// @brief Sets the base path of the application. This should be called before
/// any other function that uses the base path. Crashes if the path does not
/// exist, or if the assets folder does not exist.
/// @param path The path to set as the base path.
/// @return void
auto
set_base_path(const std::filesystem::path&) -> void;

namespace FS {

struct FileLoadError
{
  std::string message;
  enum class Code : std::uint8_t
  {
    file_not_found,
    invalid_file_format,
    unknown_error
  };
  Code code{ Code::unknown_error };
};
auto
load_binary(const std::filesystem::path&, std::vector<std::uint8_t>&)
  -> std::expected<void, FileLoadError>;
auto
load_binary(const std::filesystem::path&, std::vector<std::uint32_t>&)
  -> std::expected<void, FileLoadError>;
}