#pragma once

#include "core/extent.hpp"
#include "core/forward.hpp"

#include "renderer/layer.hpp"

#include <BS_thread_pool.hpp>
#include <expected>
#include <filesystem>
#include <memory>
#include <system_error>

namespace DynamicRendering {

struct FrametimeCalculator;

struct ApplicationArguments
{
  std::string title = "Dynamic Rendering";
  std::string working_directory = ".";
  Extent2D window_size = { 1920, 1080 };
  std::optional<std::filesystem::path> window_config_path{};
};

struct ViewportBounds
{
  glm::vec2 min{ 0.0f, 0.0f };
  glm::vec2 max{ 0.0f, 0.0f };
  [[nodiscard]] auto size() const -> glm::vec2 { return max - min; }
  [[nodiscard]] auto center() const -> glm::vec2 { return (min + max) * 0.5f; }
};

auto
parse_command_line_args(int, char**)
  -> std::expected<ApplicationArguments, std::error_code>;

class App
{
public:
  explicit App(const ApplicationArguments&);
  ~App();

  auto add_layer(std::unique_ptr<ILayer> layer) -> void;

  template<typename T, typename... Args>
  auto add_layer(Args&&... args)
  {
    if constexpr (std::is_constructible_v<T,
                                          const Device&,
                                          Renderer&,
                                          BS::priority_thread_pool*,
                                          Args...>) {
      auto l = std::make_unique<T>(
        *device, *renderer, &thread_pool, std::forward<Args>(args)...);
      layer = std::move(l);
    } else if constexpr (std::is_constructible_v<T, Device&, Args...>) {
      auto l = std::make_unique<T>(*device, std::forward<Args>(args)...);
      layer = std::move(l);
    } else {
      static_assert(false,
                    "Layer constructor must be compatible with (Device&, "
                    "[BS::thread_pool*], Args...)");
    }

    return layer.get();
  }

  auto run() -> std::error_code;

private:
  void process_events(Event&);
  void update(double dt);
  void render();
  auto interface() -> void;

  BS::thread_pool<BS::tp::priority> thread_pool;
  std::unique_ptr<Core::Instance> instance;
  std::unique_ptr<Window> window;
  std::unique_ptr<Device> device;
  std::unique_ptr<GUISystem> gui_system;
  std::unique_ptr<Swapchain> swapchain;
  std::unique_ptr<Renderer> renderer;
  std::unique_ptr<AssetReloader> asset_reloader;

  std::unique_ptr<ILayer> layer;
  bool running = true;

  std::unique_ptr<AssetFileWatcher> file_watcher;
  std::unique_ptr<FrametimeCalculator> timer;
};

}
