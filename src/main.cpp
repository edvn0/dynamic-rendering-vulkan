#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ranges>
#include <thread>
#include <vulkan/vulkan_core.h>

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

struct ILayer
{
  virtual ~ILayer() = default;
  virtual auto on_event(Event&) -> bool = 0;
  virtual auto on_interface() -> void = 0;
  virtual auto on_update(double) -> void = 0;
  virtual auto on_render(Renderer&) -> void = 0;
  virtual auto on_resize(std::uint32_t, std::uint32_t) -> void = 0;
};

struct Vertex
{
  std::array<float, 3> pos{
    0.f,
    0.f,
    0.f,
  };
  std::array<float, 3> normal{
    0.f,
    0.f,
    1.f,
  };
  std::array<float, 2> uv{
    0.f,
    0.f,
  };
};

struct Layer final : public ILayer
{
  std::unique_ptr<CommandBuffer> command_buffer;
  std::unique_ptr<GPUBuffer> vertex_buffer;
  std::unique_ptr<IndexBuffer> index_buffer;

  explicit Layer(const Device& dev)
    : command_buffer(
        CommandBuffer::create(dev,
                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT))
  {
    vertex_buffer = std::make_unique<GPUBuffer>(
      dev,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      true);

    std::array<Vertex, 4> square_vertices = {
      Vertex{
        .pos = { -0.5f, -0.5f, 0.f },
        .normal = { 0.f, 0.f, 1.f },
        .uv = { 0.f, 0.f },
      },
      Vertex{
        .pos = { 0.5f, -0.5f, 0.f },
        .normal = { 0.f, 0.f, 1.f },
        .uv = { 1.f, 0.f },
      },
      Vertex{
        .pos = { 0.5f, 0.5f, 0.f },
        .normal = { 0.f, 0.f, 1.f },
        .uv = { 1.f, 1.f },
      },
      Vertex{
        .pos = { -0.5f, 0.5f, 0.f },
        .normal = { 0.f, 0.f, 1.f },
        .uv = { 0.f, 1.f },
      },
    };
    vertex_buffer->upload(std::span(square_vertices));

    index_buffer = std::make_unique<IndexBuffer>(dev);
    std::array<std::uint32_t, 6> square_indices = {
      0, 1, 2, 2, 3, 0,
    };
    index_buffer->upload(std::span(square_indices));
  }

  auto on_event(Event& event) -> bool override
  {
    EventDispatcher dispatch(event);
    dispatch.dispatch<MouseButtonPressedEvent>([](auto&) { return true; });
    dispatch.dispatch<KeyPressedEvent>([](auto&) { return true; });
    dispatch.dispatch<WindowCloseEvent>([](auto&) { return true; });
    return false;
  }

  auto on_interface() -> void override
  {
    static constexpr auto window = [](const std::string_view name, auto&& fn) {
      ImGui::Begin(name.data());
      fn();
      ImGui::End();
    };

    static float f = 0.0f;
    static ImVec4 clr = { 0.2f, 0.3f, 0.4f, 1.0f };
    window("Controls", [] {
      ImGui::Text("Adjust settings:");
      ImGui::SliderFloat("Float", &f, 0.0f, 1.0f);
      ImGui::ColorEdit3("Clear Color", std::bit_cast<float*>(&clr));
    });

    window("GPU Timers", [&cmd = *command_buffer] {
      auto timers = cmd.resolve_timers(0);
      ImGui::Text("GPU Timers");
      for (const auto& timer : timers) {
        ImGui::Text("%s: %.3d ns",
                    timer.name.c_str(),
                    static_cast<std::int32_t>(timer.duration_ns()));
      }
    });
  }
  auto on_update(double) -> void override {}
  auto on_render(Renderer& renderer) -> void override
  {
    for (const auto i : std::views::iota(0, 100)) {
      (void)i;
      renderer.submit(DrawCommand{
        .vertex_buffer = vertex_buffer.get(),
        .index_buffer = index_buffer.get(),
      });
    }
  }

  auto on_resize(std::uint32_t, std::uint32_t) -> void override {}
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
  blueprint_registry.load_from_directory("assets/blueprints");
  PipelineFactory pipeline_factory(device);
  ComputePipelineFactory compute_pipeline_factory(device);

  GUISystem gui_system(instance, device, window);
  Swapchain swapchain(device, window);

  Renderer renderer{
    device, blueprint_registry, pipeline_factory, compute_pipeline_factory,
    window,
  };

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
      renderer.resize(width, height);
      window.set_resize_flag(false);
      continue;
    }

    std::ranges::for_each(layers, [&timer](auto& layer) {
      layer->on_update(timer.end_and_get_delta_ms());
    });
    std::ranges::for_each(
      layers, [&r = renderer](auto& layer) { layer->on_render(r); });
    renderer.end_frame(swapchain.get_frame_index());

    gui_system.begin_frame();
    std::ranges::for_each(layers, [](auto& layer) { layer->on_interface(); });

    if (ImGui::Begin("Renderer output")) {
      ImGui::Image(renderer.get_output_image().get_texture_id<ImTextureID>(),
                   ImGui::GetContentRegionAvail());
      ImGui::End();
    }

    swapchain.draw_frame(window, gui_system);
  }

  vkDeviceWaitIdle(device.get_device());
  Image::destroy_samplers();
  layers.clear();

  return 0;
}
