#include <filesystem>
#include <format>

template<>
struct std::formatter<std::filesystem::path, char>
{
  std::formatter<std::string_view, char> inner;

  constexpr auto parse(format_parse_context& ctx) { return inner.parse(ctx); }

  template<typename FormatContext>
  auto format(const std::filesystem::path& p, FormatContext& ctx) const
  {
    return inner.format(p.string(), ctx);
  }
};