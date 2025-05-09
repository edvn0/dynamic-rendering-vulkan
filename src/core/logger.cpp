#include "core/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#if FILE_LOGGING_ENABLED
#include <spdlog/sinks/basic_file_sink.h>
#endif

namespace {

inline auto
create_logger() -> std::shared_ptr<spdlog::logger>
{
  std::vector<spdlog::sink_ptr> sinks;
  sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#if FILE_LOGGING_ENABLED
  sinks.emplace_back(
    std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true));
#endif
  auto logger =
    std::make_shared<spdlog::logger>("default", sinks.begin(), sinks.end());
  logger->set_pattern("[%H:%M:%S %z] [%^%l%$] %v");
  logger->set_level(spdlog::level::debug);
  spdlog::set_default_logger(logger);
  return logger;
}

}

namespace Logger {

auto
init_logger() -> void
{
  static const auto logger = create_logger();
}

auto
log_info_impl(const std::string_view msg) -> void
{
  spdlog::info(msg);
}

auto
log_error_impl(const std::string_view msg) -> void
{
  spdlog::error(msg);
}

auto
log_warning_impl(const std::string_view msg) -> void
{
  spdlog::warn(msg);
}

auto
log_debug_impl(const std::string_view msg) -> void
{
  spdlog::debug(msg);
}

}
