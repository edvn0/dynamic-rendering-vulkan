#include "core/app.hpp"

#include "core/vulkan_util.hpp"
#include "renderer/mesh.hpp"

#include <dynamic_rendering/assets/manager.hpp>
#include <dynamic_rendering/core/asset_file_watcher.hpp>
#include <dynamic_rendering/core/asset_reloader.hpp>
#include <dynamic_rendering/core/fs.hpp>
#include <dynamic_rendering/dynamic_rendering.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <thread>

#include <GLFW/glfw3.h>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include <implot.h>
#include <lyra/lyra.hpp>
#include <tracy/Tracy.hpp>

namespace DynamicRendering {

struct FrametimeCalculator
{
  using clock = std::chrono::high_resolution_clock;

  auto get_delta_and_restart_ms() -> double
  {
    const auto now = clock::now();
    const auto delta = std::chrono::duration<double>(now - start_time);
    start_time = now;
    return delta.count();
  }

private:
  clock::time_point start_time{ clock::now() };
};

App::App(const ApplicationArguments& args)
  : thread_pool(std::thread::hardware_concurrency())
{
  ZoneScopedN("App::App");
  TracySetProgramName("Dynamic Rendering Vulkan");
  Logger::init_logger();

  {
    ZoneScopedN("Create instance");
    instance = std::make_unique<Core::Instance>(Core::Instance::create());
  }
  {
    ZoneScopedN("Create window");
    const auto path =
      args.window_config_path.value_or(get_default_config_path());
    auto config = load_window_config(path);
    config.title = args.title;
    config.size = args.window_size;

    window = Window::create(config, path);
    window->create_surface(*instance);
  }
  {
    ZoneScopedN("Create device");
    device =
      std::make_unique<Device>(Device::create(*instance, window->surface()));
    Util::Vulkan::initialise_debug_label(device->get_device());
  }

  {
    ZoneScopedN("Init image sampler cache");
    Image::init_sampler_cache(*device);
  }

  {
    ZoneScopedN("Create swapchain and GUI system");
    swapchain = std::make_unique<Swapchain>(*device, *window);
    gui_system =
      std::make_unique<GUISystem>(*instance, *device, *window, *swapchain);
  }

  {
    ZoneScopedN("Create renderer");
    renderer =
      std::make_unique<Renderer>(*device, *swapchain, *window, thread_pool);
  }

  {
    ZoneScopedN("Set event callback");
    window->set_event_callback([this](Event& event) { process_events(event); });
  }

  {
    ZoneScopedN("Create tools");
    timer = std::make_unique<FrametimeCalculator>();
  }

  {
    ZoneScopedN("Init file watcher and asset reloader");
    file_watcher = std::make_unique<AssetFileWatcher>();
    file_watcher->start_monitoring();

    asset_reloader = std::make_unique<AssetReloader>(*device, *renderer);
  }

  {
    ZoneScopedN("Init MeshCache and Asset Manager");
    MeshCache::initialise(*device);
    Assets::Manager::initialise(*device, &thread_pool);
  }
}

App::~App() = default;

void
App::add_layer(std::unique_ptr<ILayer> l)
{
  layer.swap(l);
}

auto
App::run() -> std::error_code
{
  const InitialisationParameters params{
    .window = *window,
    .swapchain = *swapchain,
    .file_watcher = *file_watcher,
    .asset_reloader = *asset_reloader,
  };
  layer->on_initialise(params);

  while (running && !window->should_close()) {
    ZoneScopedN("Main loop");

    if (window->framebuffer_resized()) {
      swapchain->request_recreate();
      auto&& [w, h] = window->framebuffer_size();
      renderer->on_resize(w, h);
      layer->on_resize(w, h);
      window->set_resize_flag(false);
      continue;
    }

    glfwPollEvents();
    if (window->is_iconified() || window->is_minimized()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    const auto dt = timer->get_delta_and_restart_ms();

    update(dt);
    interface();
    render();

    if (const auto dirty_files = file_watcher->collect_dirty();
        !dirty_files.empty()) {
      device->wait_idle();
      asset_reloader->handle_dirty_files(dirty_files);
    }
    FrameMark;
  }

  vkDeviceWaitIdle(device->get_device());
  Assets::Manager::the().clear_all<StaticMesh, Image, Material>();
  MeshCache::destroy();
  Image::destroy_samplers();
  layer->on_destroy();
  layer.reset();

  renderer.reset();
  gui_system->shutdown();
  swapchain.reset();
  window->destroy_surface({}, *instance);
  window.reset();
  device.reset();
  instance.reset();

  std::cout << "System closed safely." << std::endl;

  return {};
}

auto
App::interface() -> void
{
  ZoneScopedN("Interface");
  gui_system->begin_frame();
  layer->on_interface();
}

void
App::process_events(Event& event)
{
  EventDispatcher dispatcher(event);
  dispatcher.dispatch<WindowResizeEvent>([this](auto&) {
    window->set_resize_flag(true);
    return false;
  });
  dispatcher.dispatch<WindowCloseEvent>([this](auto&) {
    running = false;
    return false;
  });
  dispatcher.dispatch<KeyReleasedEvent>([this](auto& e) {
    if (e.key == KeyCode::Escape)
      running = false;
    return false;
  });

  layer->on_event(event);
}

void
App::update(double dt)
{
  ZoneScopedN("Update");
  layer->on_update(dt);
}

void
App::render()
{
  ZoneScopedN("Render");
  const auto frame_index = swapchain->get_frame_index();

  layer->on_render(*renderer);

  renderer->end_frame(frame_index);

  swapchain->draw_frame(*gui_system);
}

auto
parse_command_line_args(int argc, char** argv)
  -> std::expected<ApplicationArguments, std::error_code>
{
  ApplicationArguments args;
  bool asked_for_help = false;
  std::filesystem::path config_path = get_default_config_path();

  auto cli =
    lyra::opt(args.title, "title")["--title"]("Window title") |
    lyra::opt(args.working_directory,
              "dir")["--working-dir"]["--wd"]("Working directory") |
    lyra::opt(config_path,
              "path")["--window-config"]("Path to window config YAML") |
    lyra::opt(args.window_size.width, "width")["--width"]("Window width") |
    lyra::opt(args.window_size.height, "height")["--height"]("Window height");

  cli |= lyra::help(asked_for_help);

  if (const auto result = cli.parse({ argc, argv }); !result)
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));

  if (asked_for_help) {
    std::cout << cli << std::endl;
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  set_base_path(args.working_directory);

  return args;
}

} // namespace dynamic_rendering
