#pragma once

#include "core/extent.hpp"
#include "core/forward.hpp"

#include <BS_thread_pool.hpp>
#include <expected>
#include <filesystem>
#include <memory>
#include <system_error>

namespace DynamicRendering {

struct FrametimeSmoother;
struct FrametimeCalculator;
struct FrameTimePlotter;

struct ApplicationArguments
{
  std::string title = "Dynamic Rendering";
  std::string working_directory = ".";
  Extent2D window_size = { 1280, 720 };
  std::optional<std::filesystem::path> window_config_path{};
};

struct IRayPickListener
{
  virtual ~IRayPickListener() = default;
  virtual auto on_ray_pick(const glm::vec3& origin, const glm::vec3& direction)
    -> void = 0;
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
                                          Device&,
                                          BS::priority_thread_pool*,
                                          BlueprintRegistry*,
                                          Args...>) {
      auto layer = std::make_unique<T>(*device,
                                       &thread_pool,
                                       blueprint_registry.get(),
                                       std::forward<Args>(args)...);
      layers.emplace_back(std::move(layer));
    } else if constexpr (std::is_constructible_v<T,
                                                 Device&,
                                                 BS::priority_thread_pool*,
                                                 Args...>) {
      auto layer =
        std::make_unique<T>(*device, &thread_pool, std::forward<Args>(args)...);
      layers.emplace_back(std::move(layer));
    } else if constexpr (std::is_constructible_v<T, Device&, Args...>) {
      auto layer = std::make_unique<T>(*device, std::forward<Args>(args)...);
      layers.emplace_back(std::move(layer));
    } else {
      static_assert(false,
                    "Layer constructor must be compatible with (Device&, "
                    "[BS::thread_pool*], Args...)");
    }

    return layers.back().get();
  }

  auto add_ray_pick_listener(IRayPickListener* listener) -> void
  {
    ray_pick_listeners.push_back(listener);
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
  std::unique_ptr<BlueprintRegistry> blueprint_registry;
  std::unique_ptr<EditorCamera> camera;
  std::unique_ptr<AssetReloader> asset_reloader;

  std::unique_ptr<FrametimeSmoother> smoother;
  std::unique_ptr<FrameTimePlotter> plotter;
  std::unique_ptr<FrametimeCalculator> timer;

  std::vector<std::unique_ptr<ILayer>> layers;
  bool running = true;

  std::unique_ptr<AssetFileWatcher> file_watcher;
  std::vector<IRayPickListener*> ray_pick_listeners;
};

}
