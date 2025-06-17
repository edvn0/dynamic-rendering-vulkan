#pragma once

#include "core/forward.hpp"

struct InitialisationParameters
{
  const DynamicRendering::App& app;
  const Device& device;
  const Window& window;
  const Swapchain& swapchain;
  const EditorCamera& camera;
  const AssetFileWatcher& file_watcher;
  const AssetReloader& asset_reloader;
};

struct CameraMatrices
{
  glm::mat4 projection;
  glm::mat4 inverse_projection;
  glm::mat4 view;
};

struct ILayer
{
  virtual ~ILayer() = default;
  virtual auto on_initialise(const InitialisationParameters&) -> void = 0;
  virtual auto on_destroy() -> void {};
  virtual auto on_event(Event&) -> bool = 0;
  virtual auto on_interface() -> void = 0;
  virtual auto on_update(double) -> void = 0;
  virtual auto on_render(Renderer&) -> void = 0;
  virtual auto on_resize(std::uint32_t, std::uint32_t) -> void = 0;
  [[nodiscard]] virtual auto get_camera_matrices(CameraMatrices&) const -> bool
  {
    return false;
  }
};
