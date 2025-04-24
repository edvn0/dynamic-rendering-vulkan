#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <execution>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <numeric>
#include <ranges>
#include <thread>
#include <vulkan/vulkan.h>

#include "allocator.hpp"
#include "camera.hpp"
#include "command_buffer.hpp"
#include "device.hpp"
#include "gpu_buffer.hpp"
#include "gui_system.hpp"
#include "image.hpp"
#include "image_transition.hpp"
#include "imgui_impl_vulkan.h"
#include "instance.hpp"
#include "pipeline/blueprint_registry.hpp"
#include "pipeline/compute_pipeline_factory.hpp"
#include "pipeline/pipeline_factory.hpp"
#include "renderer.hpp"
#include "swapchain.hpp"
#include "window.hpp"

#include "layer.hpp"

#include "GLFW/glfw3.h"
#include "VkBootstrap.h"
#include "imgui.h"

struct FrametimeCalculator
{
  using clock = std::chrono::high_resolution_clock;

  auto start() { start_time = clock::now(); }

  auto end_and_get_delta_ms() const -> double
  {
    auto end_time = clock::now();
    auto delta =
      std::chrono::duration<double, std::milli>(end_time - start_time);
    return delta.count();
  }

private:
  clock::time_point start_time;
};

auto
main(int, char**) -> std::int32_t
{
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
  PipelineFactory pipeline_factory(device);
  ComputePipelineFactory compute_pipeline_factory(device);

  GUISystem gui_system(instance, device, window);
  Swapchain swapchain(device, window);

  Renderer renderer{
    device, blueprint_registry, pipeline_factory, compute_pipeline_factory,
    window,
  };

  Camera camera;
  auto&& [w, h] = window.framebuffer_size();
  camera.set_perspective(60.f, static_cast<float>(w) / h, 0.1f);
  camera.set_view(glm::vec3(0.f, 3.f, -2.f),
                  glm::vec3(0.f, 0.f, 0.f),
                  glm::vec3(0.f, 1.f, 0.f));
  renderer.update_frustum(camera.get_projection() * camera.get_view());

  std::vector<std::unique_ptr<ILayer>> layers;
  layers.emplace_back(std::make_unique<Layer>(device));

  auto event_callback = [&w = window, &layer_stack = layers](Event& event) {
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
    if (dispatcher.dispatch<KeyReleasedEvent>([&w](auto& e) {
          if (e.key == KeyCode::Escape)
            w.close();
          return true;
        }))
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

  FrametimeCalculator timer;
  while (!window.should_close()) {
    glfwPollEvents();
    if (window.is_iconified()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    timer.start();
    if (window.framebuffer_resized()) {
      swapchain.request_recreate(window);
      auto&& [width, height] = window.framebuffer_size();
      for (auto& layer : layers) {
        layer->on_resize(width, height);
      }
      camera.resize(width, height);
      renderer.resize(width, height);
      renderer.update_frustum(camera.get_projection() * camera.get_view());
      window.set_resize_flag(false);
      continue;
    }

    std::ranges::for_each(layers, [&timer](auto& layer) {
      layer->on_update(timer.end_and_get_delta_ms());
    });
    std::ranges::for_each(
      layers, [&r = renderer](auto& layer) { layer->on_render(r); });
    renderer.end_frame(
      swapchain.get_frame_index(), camera.get_projection(), camera.get_view());

    gui_system.begin_frame();
    std::ranges::for_each(layers, [](auto& layer) { layer->on_interface(); });

    if (ImGui::Begin("Renderer output")) {
      ImGui::Image(renderer.get_output_image().get_texture_id<ImTextureID>(),
                   ImGui::GetContentRegionAvail());
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

      // Create a table
      ImGui::BeginTable("GPU Timings", 3);
      ImGui::TableSetupColumn("Name");
      ImGui::TableSetupColumn("Duration (ms)");
      ImGui::TableSetupColumn("Command Buffer");
      ImGui::TableHeadersRow();
      // Fill the table with data
      for (const auto& section : raster_timings) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", section.name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%.2f", section.duration_ms);
        ImGui::TableNextColumn();
        ImGui::Text("Raster");
      }
      for (const auto& section : compute_timings) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", section.name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%.2f", section.duration_ms);
        ImGui::TableNextColumn();
        ImGui::Text("Compute");
      }
      // End the table
      ImGui::EndTable();

      ImGui::End();
    }

    swapchain.draw_frame(window, gui_system);
  }

  vkDeviceWaitIdle(device.get_device());
  Image::destroy_samplers();
  std::ranges::for_each(layers, [](auto& layer) { layer->on_destroy(); });
  layers.clear();

  renderer.destroy();
  // GUI system after renderer, renderer has internal images that depend on the
  // ImTextureID system
  gui_system.shutdown();

  swapchain.destroy();
  window.destroy(instance);
  device.destroy();
  instance.destroy();

  return 0;
}
