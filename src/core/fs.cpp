#include "core/fs.hpp"

#include <iostream>

namespace {

std::filesystem::path base_path_value = ".";
std::filesystem::path assets_path_value = base_path_value / "assets";

} // namespace

auto
base_path() -> const std::filesystem::path&
{
  return std::ref(base_path_value);
}

auto
assets_path() -> const std::filesystem::path&
{
  return std::ref(assets_path_value);
}

auto
set_base_path(const std::filesystem::path& path) -> void
{
  if (!std::filesystem::exists(path)) {
    Logger::log_warning("Base path does not exist: {}", path.string());
    std::abort();
  }
  if (!std::filesystem::exists(path / "assets")) {
    Logger::log_warning("Assets path does not exist: {}",
                        (path / "assets").string());
    std::abort();
  }
  base_path_value = std::filesystem::canonical(path);
  assets_path_value = base_path_value / "assets";
  Logger::log_info("Base path: {} and assets path: {}",
                   base_path_value.string(),
                   assets_path_value.string());
}

namespace FS {

auto
load_binary(const std::filesystem::path& abs_path,
            std::vector<std::uint32_t>& output)
  -> std::expected<void, FileLoadError>
{
  std::ifstream file(abs_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return std::unexpected(
      FileLoadError{ .message = "Could not open file: " + abs_path.string(),
                     .code = FileLoadError::Code::file_not_found });
  }

  const auto size = static_cast<size_t>(file.tellg());
  if (size % 4 != 0) {
    return std::unexpected(FileLoadError{
      .message = "Invalid binary file size: " + abs_path.string(),
      .code = FileLoadError::Code::invalid_file_format });
  }

  file.seekg(0);
  output.resize(size / 4);
  file.read(reinterpret_cast<char*>(output.data()), size);
  return {};
}

auto
load_binary(const std::filesystem::path& abs_path,
            std::vector<std::uint8_t>& output)
  -> std::expected<void, FileLoadError>
{
  std::vector<std::uint32_t> temp;
  if (auto result = load_binary(abs_path, temp); !result)
    return std::unexpected(result.error());

  output.resize(temp.size() * sizeof(std::uint32_t));
  std::memcpy(output.data(), temp.data(), output.size());
  return {};
}

}