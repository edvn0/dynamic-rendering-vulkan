#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

#include "command_buffer.hpp"
#include "device.hpp"
#include "gpu_buffer.hpp"
#include "gui_system.hpp"
#include "image.hpp"
#include "image_transition.hpp"
#include "instance.hpp"
#include "swapchain.hpp"
#include "window.hpp"

#include "GLFW/glfw3.h"
#include "VkBootstrap.h"
#include "imgui.h"

struct FrametimeCalculator {
  using clock = std::chrono::high_resolution_clock;

  auto start() { start_time = clock::now(); }

  auto end_and_get_delta_ms() const -> double {
    auto end_time = clock::now();
    auto delta =
        std::chrono::duration<double, std::milli>(end_time - start_time);
    return delta.count();
  }

private:
  clock::time_point start_time;
};

struct ILayer {
  virtual ~ILayer() = default;
  virtual auto on_event(Event &event) -> bool = 0;
  virtual auto on_interface() -> void = 0;
  virtual auto on_update(double delta_time) -> void = 0;
  virtual auto on_render(std::uint32_t frame_index) -> void = 0;
};

auto main(int, char **) -> std::int32_t {
  auto instance = Core::Instance::create();
  Window window;
  window.create_surface(instance);
  auto device = Device::create(instance, window.surface());

  GUISystem gui_system(instance, device, window);
  Swapchain swapchain(device, window);

  struct Layer final : public ILayer {
    std::unique_ptr<CommandBuffer> command_buffer;
    std::unique_ptr<Image> offscreen_image;
    std::unique_ptr<GpuBuffer> vertex_buffer;

    explicit Layer(const Device &dev)
        : command_buffer(CommandBuffer::create(dev)) {

      offscreen_image =
          Image::create(dev, ImageConfiguration{
                                 .extent = {800, 600},
                                 .format = VK_FORMAT_B8G8R8A8_SRGB,
                             });

      vertex_buffer = std::make_unique<GpuBuffer>(
          dev,
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          true);

      struct Vertex {
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

      std::array<Vertex, 3> vertices = {
          Vertex{{0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
          Vertex{{1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f}},
          Vertex{{0.5f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {0.5f, 1.f}},
      };
      vertex_buffer->upload<Vertex>(std::span(vertices));
    }

    auto on_event(Event &event) -> bool override {
      EventDispatcher dispatch(event);
      dispatch.dispatch<MouseButtonPressedEvent>([](auto &) { return true; });
      dispatch.dispatch<KeyPressedEvent>([](auto &) { return true; });
      dispatch.dispatch<WindowCloseEvent>([](auto &) { return true; });
      return false;
    }

    auto on_interface() -> void override {
      static constexpr auto window = [](const std::string_view name,
                                        auto &&fn) {
        ImGui::Begin(name.data());
        fn();
        ImGui::End();
      };

      static bool show_demo = true;
      ImGui::ShowDemoWindow(&show_demo);

      static float f = 0.0f;
      static ImVec4 clr = {0.2f, 0.3f, 0.4f, 1.0f};
      window("Controls", [] {
        ImGui::Text("Adjust settings:");
        ImGui::Checkbox("Demo Window", &show_demo);
        ImGui::SliderFloat("Float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit3("Clear Color", std::bit_cast<float *>(&clr));
      });

      window("GPU Timers", [&cmd = *command_buffer] {
        auto timers = cmd.resolve_timers(0);
        ImGui::Text("GPU Timers");
        for (const auto &timer : timers) {
          ImGui::Text("%s: %.3d ns", timer.name.c_str(),
                      static_cast<std::int32_t>(timer.duration_ns()));
        }
      });
    }
    auto on_update(double) -> void override {}
    auto on_render(std::uint32_t frame_index) -> void override {
      command_buffer->begin_frame(frame_index);
      command_buffer->begin_timer(frame_index, "geometry_pass");

      {
        const VkCommandBuffer cmd = command_buffer->get(frame_index);

        VkClearValue clear_value = {.color = {{0.f, 0.f, 0.f, 1.f}}};

        VkRenderingAttachmentInfo color_attachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = offscreen_image->get_view(),
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clear_value,
        };

        VkRenderingInfo render_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea =
                {
                    .offset = {0, 0},
                    .extent =
                        {
                            offscreen_image->width(),
                            offscreen_image->height(),
                        },
                },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
        };

        vkCmdBeginRendering(cmd, &render_info);
        //         vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        //         pipeline);
        auto offsets = std::array<VkDeviceSize, 1>{0};
        const std::array<VkBuffer, 1> vertex_buffers{
            vertex_buffer->get(),
        };
        vkCmdBindVertexBuffers(
            cmd, 0, static_cast<std::uint32_t>(vertex_buffers.size()),
            vertex_buffers.data(), offsets.data());
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRendering(cmd);
      }

      command_buffer->end_timer(frame_index, "geometry_pass");
      command_buffer->begin_timer(frame_index, "gui_pass");

      // ...record GUI draw calls

      command_buffer->end_timer(frame_index, "gui_pass");
      command_buffer->submit_and_end(frame_index);
    }
  };

  std::vector<std::unique_ptr<ILayer>> layers;
  layers.emplace_back(std::make_unique<Layer>(device));

  auto event_callback = [&w = window, &layer_stack = layers](Event &event) {
    EventDispatcher dispatcher(event);
    if (dispatcher.dispatch<WindowResizeEvent>([&w](auto &) {
          w.set_resize_flag(true);
          return true;
        }))
      return;
    if (dispatcher.dispatch<WindowCloseEvent>([&w](auto &) {
          w.close();
          return true;
        }))
      return;
    if (dispatcher.dispatch<KeyReleasedEvent>([&w](auto &e) {
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
      window.set_resize_flag(false);
    }

    std::ranges::for_each(layers, [&timer](auto &layer) {
      layer->on_update(timer.end_and_get_delta_ms());
    });
    std::ranges::for_each(layers, [&sc = swapchain](auto &layer) {
      layer->on_render(sc.get_frame_index());
    });

    gui_system.begin_frame();
    std::ranges::for_each(layers, [](auto &layer) { layer->on_interface(); });
    swapchain.draw_frame(window, gui_system);
  }

  vkDeviceWaitIdle(device.get_device());

  return 0;
}
