#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <ranges>
#include <thread>
#include <vulkan/vulkan.h>
#include <execution>
#include <numeric>


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

#include "GLFW/glfw3.h"
#include "VkBootstrap.h"
#include "imgui.h"

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

inline auto
generate_cube(const Device& device)
  -> std::tuple<std::unique_ptr<GPUBuffer>, std::unique_ptr<IndexBuffer>>
{
  std::array<Vertex, 24> vertices = {
    // Front face (+Z)
    Vertex{ { -0.5f, -0.5f, 0.5f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, 0.5f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, 0.5f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, 0.5f, 0.5f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },

    // Back face (−Z)
    Vertex{ { 0.5f, -0.5f, -0.5f }, { 0.f, 0.f, -1.f }, { 0.f, 0.f } },
    Vertex{ { -0.5f, -0.5f, -0.5f }, { 0.f, 0.f, -1.f }, { 1.f, 0.f } },
    Vertex{ { -0.5f, 0.5f, -0.5f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f } },
    Vertex{ { 0.5f, 0.5f, -0.5f }, { 0.f, 0.f, -1.f }, { 0.f, 1.f } },

    // Left face (−X)
    Vertex{ { -0.5f, -0.5f, -0.5f }, { -1.f, 0.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { -0.5f, -0.5f, 0.5f }, { -1.f, 0.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { -0.5f, 0.5f, 0.5f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, 0.5f, -0.5f }, { -1.f, 0.f, 0.f }, { 0.f, 1.f } },

    // Right face (+X)
    Vertex{ { 0.5f, -0.5f, 0.5f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, -0.5f }, { 1.f, 0.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, -0.5f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f } },
    Vertex{ { 0.5f, 0.5f, 0.5f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f } },

    // Top face (+Y)
    Vertex{ { -0.5f, 0.5f, 0.5f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, 0.5f }, { 0.f, 1.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, 0.5f, -0.5f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, 0.5f, -0.5f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f } },

    // Bottom face (−Y)
    Vertex{ { -0.5f, -0.5f, -0.5f }, { 0.f, -1.f, 0.f }, { 0.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, -0.5f }, { 0.f, -1.f, 0.f }, { 1.f, 0.f } },
    Vertex{ { 0.5f, -0.5f, 0.5f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f } },
    Vertex{ { -0.5f, -0.5f, 0.5f }, { 0.f, -1.f, 0.f }, { 0.f, 1.f } },
  };

  std::array<std::uint32_t, 36> indices = {
    0,  1,  2,  2,  3,  0,  // front
    4,  5,  6,  6,  7,  4,  // back
    8,  9,  10, 10, 11, 8,  // left
    12, 13, 14, 14, 15, 12, // right
    16, 17, 18, 18, 19, 16, // top
    20, 21, 22, 22, 23, 20  // bottom
  };

  auto vertex_buffer = std::make_unique<GPUBuffer>(
    device,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    true);
  vertex_buffer->upload(std::span(vertices));

  auto index_buffer = std::make_unique<IndexBuffer>(device);
  index_buffer->upload(std::span(indices));

  return { std::move(vertex_buffer), std::move(index_buffer) };
}

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
  virtual auto on_destroy() -> void {};
  virtual auto on_event(Event&) -> bool = 0;
  virtual auto on_interface() -> void = 0;
  virtual auto on_update(double) -> void = 0;
  virtual auto on_render(Renderer&) -> void = 0;
  virtual auto on_resize(std::uint32_t, std::uint32_t) -> void = 0;
};

struct Layer final : public ILayer
{
  std::unique_ptr<GPUBuffer> vertex_buffer;
  std::unique_ptr<IndexBuffer> index_buffer;
  std::unique_ptr<GPUBuffer> cube_vertex_buffer;
  std::unique_ptr<IndexBuffer> cube_index_buffer;
  std::vector<glm::mat4> transforms{};
  glm::vec2 bounds{ 0.F };
  float rotation_speed = 3.6f;

  auto on_destroy() -> void override
  {
    vertex_buffer.reset();
    index_buffer.reset();
  }

  explicit Layer(const Device& dev)
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

    transforms.resize(100'000);

    auto&& [cube_vertex, cube_index] = generate_cube(dev);
    cube_vertex_buffer = std::move(cube_vertex);
    cube_index_buffer = std::move(cube_index);
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

    static ImVec4 clr = { 0.2f, 0.3f, 0.4f, 1.0f };
    window("Controls", [&rs = rotation_speed] {
      ImGui::Text("Adjust settings:");
      ImGui::ColorEdit3("Clear Color", std::bit_cast<float*>(&clr));
      ImGui::SliderFloat("Rotation Speed", &rs, 0.1f, 150.0f);
    });
  }
  auto on_update(double ts) -> void override
  {
    static float angle = 0.f;
    angle += rotation_speed * static_cast<float>(ts);
    angle = std::fmod(angle, 360.f);

    std::for_each(
      std::execution::par_unseq,
      transforms.begin(),
      transforms.end(),
      [root = transforms.data()](glm::mat4& mat) {
        const std::size_t i = &mat - root;
        const float x = static_cast<float>(i % 10) - 5.f;
        const float y = static_cast<float>(i) / 10.F - 5.f;
        mat =
          glm::translate(glm::mat4(1.f), { x * 2.f, y * 2.f, 0.f }) *
          glm::rotate(glm::mat4(1.f), glm::radians(angle), { 0.f, 0.f, 1.f });
      });
  }

  auto on_render(Renderer& renderer) -> void override
  {
    for (const auto& transform : transforms) {
      renderer.submit(
        {
          .vertex_buffer = vertex_buffer.get(),
          .index_buffer = index_buffer.get(),
        },
        transform);
    }

    renderer.submit(
      {
        .vertex_buffer = cube_vertex_buffer.get(),
        .index_buffer = cube_index_buffer.get(),
      },
      transforms.at(0));
  }

  auto on_resize(std::uint32_t w, std::uint32_t h) -> void override
  {
    bounds.x = static_cast<float>(w);
    bounds.y = static_cast<float>(h);
  }
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
    blueprint_registry.load_from_directory(fs{ "assets" } /
                                           fs{ "blueprints" });
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
