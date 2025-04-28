#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <execution>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <numeric>
#include <ranges>
#include <thread>
#include <vulkan/vulkan.h>

#include <dynamic_rendering/dynamic_rendering.hpp>

#include "GLFW/glfw3.h"
#include "imgui.h"
#include <implot.h>

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

      const float target_line[] = { target_frametime_ms, target_frametime_ms };
      const float x_positions[] = { 0.0f, static_cast<float>(count - 1) };

      ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 1.0f, 0.0f, 0.5f));
      ImPlot::PlotLine("60 FPS Line", x_positions, target_line, 2);
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

auto
main(int argc, char** argv) -> std::int32_t
{
  if (argc > 1) {
    if (!std::filesystem::exists(argv[1]))
      assert(false && "Could not find CWD to set");

    std::filesystem::current_path(argv[1]);
  }

  auto instance = Core::Instance::create();
  Window window;
  window.create_surface(instance);
  auto device = Device::create(instance, window.surface());
  Image::init_sampler_cache(device);

  BlueprintRegistry blueprint_registry;
  {
    using fs = std::filesystem::path;
    blueprint_registry.load_from_directory(fs{ "assets" } / fs{ "blueprints" });
  }
  GUISystem gui_system(instance, device, window);
  Swapchain swapchain(device, window);

  Renderer renderer{
    device,
    blueprint_registry,
    window,
  };

  auto&& [w, h] = window.framebuffer_size();
  EditorCamera camera{ 90.0F, static_cast<float>(w) / h, 0.1F, 1000.0F };
  renderer.update_frustum(camera.get_projection() * camera.get_view());

  std::vector<std::unique_ptr<ILayer>> layers;
  layers.emplace_back(std::make_unique<Layer>(device));

  auto event_callback =
    [&w = window, &layer_stack = layers, &camera](Event& event) {
      EventDispatcher dispatcher(event);
      if (dispatcher.dispatch<WindowResizeEvent>([&w](auto&) {
            w.set_resize_flag(true);
            return true;
          }))
        return;
      if (dispatcher.dispatch<WindowCloseEvent>([&w](auto&) {
            w.close();
            return true;
          }))
        return;
      if (dispatcher.dispatch<KeyReleasedEvent>([&w, &c = camera](auto& e) {
            if (e.key == KeyCode::Escape)
              w.close();
            if (e.key == KeyCode::L) {
              auto&& [width, height] = w.framebuffer_size();
              const float aspect_ratio = static_cast<float>(width) / height;

              if (std::holds_alternative<Camera::InfiniteProjection>(
                    c.get_projection_config())) {
                c.set_perspective_float_far(90.f, aspect_ratio, 0.1f, 500.f);
              } else {
                c.set_perspective(90.f, aspect_ratio, 0.1f);
              }
            }
            return true;
          }))
        return;

      if (camera.on_event(event))
        return;

      for (auto it = layer_stack.end(); it != layer_stack.begin();) {
        if ((*--it)->on_event(event))
          break;
      }
      if (event.handled)
        return;

      // 3) Any other ad‑hoc callbacks you’ve registered
      /* for (auto &cb : event_callbacks_) {
        cb(event);
        if (event.handled)
          break;
      } */
    };
  window.set_event_callback(std::move(event_callback));

  FrametimeSmoother timer_smoother;
  FrameTimePlotter frame_time_plotter;
  FrametimeCalculator timer;
  while (!window.should_close()) {
    if (window.framebuffer_resized()) {
      swapchain.request_recreate();
      auto&& [width, height] = window.framebuffer_size();
      for (auto& layer : layers) {
        layer->on_resize(width, height);
      }
      camera.resize(width, height);
      renderer.resize(width, height);
      window.set_resize_flag(false);
      continue;
    }

    glfwPollEvents();
    if (window.is_iconified()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    const auto dt = timer.get_delta_and_restart_ms();
    timer_smoother.add_sample(dt);
    frame_time_plotter.add_sample(dt);

    camera.on_update(dt);
    std::ranges::for_each(layers, [&dt](auto& layer) { layer->on_update(dt); });
    const auto frame_index = swapchain.get_frame_index();
    renderer.begin_frame(frame_index,
                         camera.compute_view_projection(),
                         camera.compute_inverse_view_projection());
    std::ranges::for_each(
      layers, [&r = renderer](auto& layer) { layer->on_render(r); });
    renderer.end_frame(frame_index);

    gui_system.begin_frame();
    std::ranges::for_each(layers, [](auto& layer) { layer->on_interface(); });

    constexpr auto flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBackground;

    if (ImGui::Begin("Renderer output", nullptr, flags)) {
      auto size = ImGui::GetWindowSize();

      ImGui::Image(renderer.get_output_image().get_texture_id<ImTextureID>(),
                   size);

      ImGui::End();
    }

    if (ImGui::Begin("Shadow output", nullptr, flags)) {
      auto size = ImGui::GetWindowSize();

      ImGui::Image(renderer.get_shadow_image().get_texture_id<ImTextureID>(),
                   size);
      ImGui::End();
    }

    if (ImGui::Begin("Performance Metrics")) {
      frame_time_plotter.plot();
      ImGui::Text("Smoothed FPS: %.2f", 1.0 / timer_smoother.get_smoothed());
      ImGui::Text("Smoothed Frame Time: %.2f ms",
                  timer_smoother.get_smoothed() * 1000.0);
      ImGui::End();
    }

    if (ImGui::Begin("GPU Timers")) {
      const auto& command_buffer = renderer.get_command_buffer();
      const auto& compute_command_buffer =
        renderer.get_compute_command_buffer();

      auto raster_timings =
        command_buffer.resolve_timers(swapchain.get_frame_index());
      auto compute_timings =
        compute_command_buffer.resolve_timers(swapchain.get_frame_index());

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

    swapchain.draw_frame(gui_system);
  }

  vkDeviceWaitIdle(device.get_device());
  Image::destroy_samplers();
  std::ranges::for_each(layers, [](auto& layer) { layer->on_destroy(); });
  layers.clear();

  renderer.destroy();
  // GUI system after renderer, renderer has internal images that depend on
  // the ImTextureID system
  gui_system.shutdown();

  swapchain.destroy();
  window.destroy(instance);
  device.destroy();
  instance.destroy();

  std::cout << "System could be closed safely" << std::endl;

  return 0;
}
