#include "core/app.hpp"

#include "renderer/mesh.hpp"

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
#include <imgui.h>
#include <implot.h>
#include <lyra/lyra.hpp>
#include <tracy/Tracy.hpp>

namespace DynamicRendering {

struct FrameTimePlotter
{
  static constexpr std::size_t history_size = 1000;
  std::array<float, history_size> history{};
  std::size_t index = 0;
  bool filled_once = false;

  static constexpr float target_frametime_ms = 16.6667f; // 60 FPS = 16.66ms

  auto add_sample(double frame_time_seconds) -> void
  {
    history[index] = static_cast<float>(frame_time_seconds * 1000.0);
    index = (index + 1) % history_size;
    if (index == 0)
      filled_once = true;
  }

  auto plot(const char* label = "Frametime (ms)") const -> void
  {
    const auto* data = history.data();
    const std::size_t count = filled_once ? history_size : index;

    if (ImPlot::BeginPlot(label, ImVec2(-1, 150))) {
      ImPlot::SetupAxes(nullptr,
                        "Frame Time (ms)",
                        ImPlotAxisFlags_NoGridLines,
                        ImPlotAxisFlags_AutoFit);
      ImPlot::SetupAxes("Frame #",
                        "Frame Time (ms)",
                        ImPlotAxisFlags_NoGridLines |
                          ImPlotAxisFlags_NoTickLabels,
                        ImPlotAxisFlags_NoGridLines);

      ImPlot::SetupAxisLimits(
        ImAxis_X1, 0, static_cast<double>(history_size), ImGuiCond_Always);
      ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 5.0, ImGuiCond_Always);

      ImPlot::PlotLine("Frametime",
                       data,
                       static_cast<int>(count),
                       1.0,
                       0,
                       ImPlotLineFlags_None);

      const std::array target_line = { target_frametime_ms,
                                       target_frametime_ms };
      const std::array x_positions = { 0.0f, static_cast<float>(count - 1) };

      ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 1.0f, 0.0f, 0.5f));
      ImPlot::PlotLine(
        "60 FPS Line", x_positions.data(), target_line.data(), 2);
      ImPlot::PopStyleColor();

      ImPlot::EndPlot();
    }
  }
};

struct FrametimeSmoother
{
  double smoothed_dt = 0.0;
  static constexpr double alpha = 0.1;

  auto add_sample(double frame_time) -> void
  {
    if (smoothed_dt == 0.0)
      smoothed_dt = frame_time;
    else
      smoothed_dt = alpha * frame_time + (1.0 - alpha) * smoothed_dt;
  }

  auto get_smoothed() const -> double { return smoothed_dt; }
};

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
  }

  Image::init_sampler_cache(*device);

  blueprint_registry = std::make_unique<BlueprintRegistry>();
  blueprint_registry->load_from_directory("blueprints");

  gui_system = std::make_unique<GUISystem>(*instance, *device, *window);
  swapchain = std::make_unique<Swapchain>(*device, *window);
  renderer = std::make_unique<Renderer>(
    *device, *blueprint_registry, *window, thread_pool);

  auto&& [w, h] = window->framebuffer_size();
  camera = std::make_unique<EditorCamera>(
    90.0F, static_cast<float>(w) / static_cast<float>(h), 0.1F, 1000.0F);
  renderer->update_frustum(camera->get_projection() * camera->get_view());

  window->set_event_callback([this](Event& event) { process_events(event); });

  smoother = std::make_unique<FrametimeSmoother>();
  plotter = std::make_unique<FrameTimePlotter>();
  timer = std::make_unique<FrametimeCalculator>();

  file_watcher = std::make_unique<AssetFileWatcher>();
  file_watcher->start_monitoring();

  asset_reloader =
    std::make_unique<AssetReloader>(*blueprint_registry, *renderer);

  MeshCache::initialise(*device, *blueprint_registry);
}

App::~App() = default;

void
App::add_layer(std::unique_ptr<ILayer> layer)
{
  layers.emplace_back(std::move(layer));
}

auto
App::run() -> std::error_code
{
  while (running && !window->should_close()) {
    ZoneScopedN("Main loop");

    if (window->framebuffer_resized()) {
      swapchain->request_recreate();
      auto&& [w, h] = window->framebuffer_size();
      camera->resize(w, h);
      renderer->resize(w, h);
      for (const auto& layer : layers)
        layer->on_resize(w, h);
      window->set_resize_flag(false);
      continue;
    }

    glfwPollEvents();
    if (window->is_iconified()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    const auto dt = timer->get_delta_and_restart_ms();

    {
      ZoneScopedN("Add samples to smoothers");
      smoother->add_sample(dt);
      plotter->add_sample(dt);
    }
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
  MeshCache::destroy();
  Image::destroy_samplers();
  for (const auto& layer : layers)
    layer->on_destroy();

  layers.clear();

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
  gui_system->begin_frame();
  for (const auto& layer : layers)
    layer->on_interface();

  constexpr auto flags =
    (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
     ImGuiWindowFlags_NoBackground) &
    0;

  if (ImGui::Begin("Renderer Output", nullptr, flags)) {
    auto size = ImGui::GetWindowSize();
    ImGui::Image(renderer->get_output_image().get_texture_id<ImTextureID>(),
                 size);
    ImGui::End();
  }

  if (ImGui::Begin("Shadow Output", nullptr, flags)) {
    auto size = ImGui::GetWindowSize();
    ImGui::Image(renderer->get_shadow_image().get_texture_id<ImTextureID>(),
                 size);
    ImGui::End();
  }

  if (ImGui::Begin("Performance Metrics")) {
    plotter->plot();
    ImGui::Text("Smoothed FPS: %.2f", 1.0 / smoother->get_smoothed());
    ImGui::Text("Smoothed Frame Time: %.2f ms",
                smoother->get_smoothed() * 1000.0);
    ImGui::End();
  }

  if (ImGui::Begin("GPU Timers")) {
    const auto& command_buffer = renderer->get_command_buffer();
    const auto& compute_command_buffer = renderer->get_compute_command_buffer();

    auto raster_timings =
      command_buffer.resolve_timers(swapchain->get_frame_index());
    auto compute_timings =
      compute_command_buffer.resolve_timers(swapchain->get_frame_index());

    ImGui::BeginTable("GPU Timings", 3);
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Duration (us)");
    ImGui::TableSetupColumn("Command Buffer");
    ImGui::TableHeadersRow();

    for (const auto& section : raster_timings) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", section.name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%.0f", section.duration_ms * 1000.0); // <-- MICROSECONDS
      ImGui::TableNextColumn();
      ImGui::Text("Raster");
    }

    for (const auto& section : compute_timings) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", section.name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%.0f", section.duration_ms * 1000.0); // <-- MICROSECONDS
      ImGui::TableNextColumn();
      ImGui::Text("Compute");
    }

    ImGui::EndTable();
    ImGui::End();
  }
}

void
App::process_events(Event& event)
{
  EventDispatcher dispatcher(event);
  dispatcher.dispatch<WindowResizeEvent>([this](auto&) {
    window->set_resize_flag(true);
    return true;
  });
  dispatcher.dispatch<WindowCloseEvent>([this](auto&) {
    running = false;
    return true;
  });
  dispatcher.dispatch<KeyReleasedEvent>([this](auto& e) {
    if (e.key == KeyCode::Escape)
      running = false;
    if (e.key == KeyCode::L) {
      auto&& [width, height] = window->framebuffer_size();
      const float aspect_ratio =
        static_cast<float>(width) / static_cast<float>(height);

      if (std::holds_alternative<Camera::InfiniteProjection>(
            camera->get_projection_config())) {
        camera->set_perspective_float_far(90.f, aspect_ratio, 0.1f, 500.f);
      } else {
        camera->set_perspective(90.f, aspect_ratio, 0.1f);
      }
    }
    return true;
  });

  if (camera->on_event(event))
    return;

  for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
    if ((*it)->on_event(event))
      break;
  }
}

void
App::update(double dt)
{
  camera->on_update(dt);
  for (const auto& layer : layers)
    layer->on_update(dt);
}

void
App::render()
{
  const auto frame_index = swapchain->get_frame_index();

  renderer->begin_frame(
    frame_index,
    {
      .projection = camera->get_projection(),
      .inverse_projection = camera->get_inverse_projection(),
      .view = camera->get_view(),
    });

  for (const auto& layer : layers)
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
