#pragma once

#include "core/forward.hpp"

#include <memory>
#include <string_view>

namespace DynamicRendering {

struct FrametimeSmoother;
struct FrametimeCalculator;
struct FrameTimePlotter;

class App
{
public:
  explicit App(std::string_view = "Dynamic Rendering App");
  ~App();

  auto add_layer(std::unique_ptr<ILayer> layer) -> void;

  template<typename T, typename... Args>
  auto add_layer(Args&&... args) -> void
  {
    static_assert(std::derived_from<T, ILayer>);
    layers.emplace_back(
      std::make_unique<T>(*device, std::forward<Args>(args)...));
  }

  auto run(int argc, char** argv) -> std::error_code;

private:
  void process_events(Event&);
  void update(double dt);
  void render();
  auto interface() -> void;

private:
  std::unique_ptr<Core::Instance> instance;
  std::unique_ptr<Window> window;
  std::unique_ptr<Device> device;
  std::unique_ptr<GUISystem> gui_system;
  std::unique_ptr<Swapchain> swapchain;
  std::unique_ptr<Renderer> renderer;
  std::unique_ptr<BlueprintRegistry> blueprint_registry;
  std::unique_ptr<EditorCamera> camera;

  std::unique_ptr<FrametimeSmoother> smoother;
  std::unique_ptr<FrameTimePlotter> plotter;
  std::unique_ptr<FrametimeCalculator> timer;

  std::vector<std::unique_ptr<ILayer>> layers;
  bool running = true;

  std::unique_ptr<MaterialYAMLFileWatcher> file_watcher;
  std::unordered_map<std::string, std::string> filename_to_material_name;
};

}