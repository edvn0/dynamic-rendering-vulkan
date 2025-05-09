#pragma once

#include <format>
#include <string>
#include <string_view>

namespace Logger {

auto
init_logger() -> void;
auto log_info_impl(std::string_view) -> void;
auto log_error_impl(std::string_view) -> void;
auto log_warning_impl(std::string_view) -> void;
auto log_debug_impl(std::string_view) -> void;

template<typename... Args>
inline auto
log_info(const std::format_string<Args...>& fmt, Args&&... args) -> void
{
  log_info_impl(std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline auto
log_error(const std::format_string<Args...>& fmt, Args&&... args) -> void
{
  log_error_impl(std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline auto
log_warning(const std::format_string<Args...>& fmt, Args&&... args) -> void
{
  log_warning_impl(std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline auto
log_debug(const std::format_string<Args...>& fmt, Args&&... args) -> void
{
  log_debug_impl(std::format(fmt, std::forward<Args>(args)...));
}

}
