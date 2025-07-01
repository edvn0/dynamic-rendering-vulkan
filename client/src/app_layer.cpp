#include "app_layer.hpp"

#include "dynamic_rendering/assets/manager.hpp"
#include <dynamic_rendering/core/fs.hpp>

#include <array>
#include <execution>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <numeric>
#include <span>

// clang-format off
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <latch>
#include <random>
#include <tracy/Tracy.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <implot.h>
// clang-format on

/// @brief Real-time frame time visualization utility for performance monitoring
///
/// Maintains a circular buffer of frame time samples and provides ImPlot
/// visualization with configurable display options and performance target
/// indicators.
struct FrameTimePlotter
{
  /// @brief Maximum number of frame time samples to store
  static constexpr std::size_t HISTORY_SIZE = 1000;

  /// @brief Target frame time for 60 FPS in milliseconds
  static constexpr float TARGET_60_FPS_MS = 16.6667f;

  /// @brief Target frame time for 30 FPS in milliseconds
  static constexpr float TARGET_30_FPS_MS = 33.3333f;

  /// @brief Default plot height in pixels
  static constexpr float DEFAULT_PLOT_HEIGHT = 150.0f;

private:
  std::array<float, HISTORY_SIZE> frame_times_{};
  std::size_t write_index_{ 0 };
  bool buffer_full_{ false };

public:
  /// @brief Add a new frame time sample to the circular buffer
  /// @param frame_time_seconds Frame time in seconds
  auto add_sample(double frame_time_seconds) noexcept -> void
  {
    frame_times_[write_index_] =
      static_cast<float>(frame_time_seconds * 1000.0);
    write_index_ = (write_index_ + 1) % HISTORY_SIZE;

    if (write_index_ == 0) {
      buffer_full_ = true;
    }
  }

  /// @brief Get the current number of valid samples in the buffer
  /// @return Number of samples currently stored
  [[nodiscard]] auto get_sample_count() const noexcept -> std::size_t
  {
    return buffer_full_ ? HISTORY_SIZE : write_index_;
  }

  /// @brief Calculate average frame time from current samples
  /// @return Average frame time in milliseconds, or 0.0 if no samples
  [[nodiscard]] auto get_average_frame_time() const noexcept -> float
  {
    const auto count = get_sample_count();
    if (count == 0)
      return 0.0f;

    const auto sum =
      std::accumulate(frame_times_.begin(), frame_times_.begin() + count, 0.0f);
    return sum / static_cast<float>(count);
  }

  /// @brief Get the most recent frame time sample
  /// @return Latest frame time in milliseconds, or 0.0 if no samples
  [[nodiscard]] auto get_current_frame_time() const noexcept -> float
  {
    if (get_sample_count() == 0)
      return 0.0f;

    const auto last_index =
      write_index_ == 0 ? HISTORY_SIZE - 1 : write_index_ - 1;
    return frame_times_[last_index];
  }

  /// @brief Clear all samples and reset the buffer
  auto clear() noexcept -> void
  {
    frame_times_.fill(0.0f);
    write_index_ = 0;
    buffer_full_ = false;
  }

  /// @brief Render the frame time plot using ImPlot
  /// @param label Plot title and window label
  /// @param plot_height Height of the plot in pixels
  /// @param show_target_lines Whether to display FPS target reference lines
  auto plot(std::string_view label = "Frame Time Analysis",
            float plot_height = DEFAULT_PLOT_HEIGHT,
            bool show_target_lines = true) const -> void
  {

    const auto sample_count = get_sample_count();
    if (sample_count == 0)
      return;

    if (ImPlot::BeginPlot(label.data(), ImVec2(-1, plot_height))) {
      setup_plot_axes();
      plot_frame_time_data(sample_count);

      if (show_target_lines) {
        plot_target_reference_lines(sample_count);
      }

      ImPlot::EndPlot();
    }
  }

private:
  /// @brief Configure plot axes with appropriate styling and limits
  auto setup_plot_axes() const -> void
  {
    // Setup X-axis (frame index)
    ImPlot::SetupAxis(ImAxis_X1,
                      "Frame Index",
                      ImPlotAxisFlags_NoGridLines |
                        ImPlotAxisFlags_NoTickLabels);

    // Setup Y-axis (frame time in ms)
    ImPlot::SetupAxis(ImAxis_Y1, "Frame Time (ms)", ImPlotAxisFlags_AutoFit);

    // Set axis limits
    ImPlot::SetupAxisLimits(
      ImAxis_X1, 0.0, static_cast<double>(HISTORY_SIZE), ImGuiCond_Always);

    // Dynamic Y-axis scaling based on current data
    const auto max_reasonable_time =
      std::max(50.0f, get_average_frame_time() * 2.0f);
    ImPlot::SetupAxisLimits(
      ImAxis_Y1, 0.0, static_cast<double>(max_reasonable_time), ImGuiCond_Once);
  }

  /// @brief Plot the actual frame time data
  /// @param sample_count Number of valid samples to plot
  auto plot_frame_time_data(std::size_t sample_count) const -> void
  {
    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImPlot::PlotLine("Frame Time",
                     frame_times_.data(),
                     static_cast<int>(sample_count),
                     1.0,
                     0.0,
                     ImPlotLineFlags_None);
    ImPlot::PopStyleColor();
  }

  /// @brief Plot reference lines for common FPS targets
  /// @param sample_count Number of samples (for line length)
  auto plot_target_reference_lines(std::size_t sample_count) const -> void
  {
    const std::array<float, 2> x_range = {
      0.0f, static_cast<float>(sample_count - 1)
    };

    // 60 FPS target line
    plot_reference_line("60 FPS Target",
                        x_range,
                        TARGET_60_FPS_MS,
                        ImVec4(1.0f, 1.0f, 0.0f, 0.7f));

    // 30 FPS target line
    plot_reference_line("30 FPS Target",
                        x_range,
                        TARGET_30_FPS_MS,
                        ImVec4(1.0f, 0.5f, 0.0f, 0.5f));
  }

  /// @brief Plot a single horizontal reference line
  /// @param label Line label for legend
  /// @param x_range X-axis range for the line
  /// @param y_value Y-axis value (frame time in ms)
  /// @param color Line color
  auto plot_reference_line(const char* label,
                           const std::array<float, 2>& x_range,
                           float y_value,
                           const ImVec4& color) const -> void
  {
    const std::array<float, 2> y_values = { y_value, y_value };

    ImPlot::PushStyleColor(ImPlotCol_Line, color);
    ImPlot::PlotLine(label, x_range.data(), y_values.data(), 2);
    ImPlot::PopStyleColor();
  }
};

struct FrametimeSmoother
{
  double smoothed_dt = 0.0;
  static constexpr double alpha = 0.1;

  auto add_sample(const double frame_time) -> void
  {
    if (smoothed_dt == 0.0)
      smoothed_dt = frame_time;
    else
      smoothed_dt = alpha * frame_time + (1.0 - alpha) * smoothed_dt;
  }

  [[nodiscard]] auto get_smoothed() const -> double { return smoothed_dt; }
};

auto
generate_world_ray(const glm::vec2& local_mouse,
                   const glm::vec2& viewport_size,
                   const glm::vec3& camera_pos,
                   const glm::mat4& view,
                   const glm::mat4& projection)
  -> std::pair<glm::vec3, glm::vec3>
{
  const auto x_ndc = (2.0f * local_mouse.x) / viewport_size.x - 1.0f;
  const auto y_ndc = 1.0f - (2.0f * local_mouse.y) / viewport_size.y;

  const auto ray_clip =
    glm::vec4(x_ndc, y_ndc, 0.0f, 1.0f); // Z = 0.0f for near plane
  auto ray_view = glm::inverse(projection) * ray_clip;
  ray_view /= ray_view.w;

  const auto ray_world = glm::inverse(view) * ray_view;
  const auto ray_direction = glm::normalize(glm::vec3(ray_world) - camera_pos);

  return { camera_pos, ray_direction };
}

struct CubeComponent
{
  static constexpr auto name = "CubeComponent";
  bool is_active{ true };
};

AppLayer::AppLayer(const Device&, Renderer& r, BS::priority_thread_pool* pool)
  : thread_pool(pool)
  , renderer(&r)
  , file_browser(assets_path())
{
  active_scene = std::make_shared<Scene>("Basic");

  auto cerberus = active_scene->create_entity("Cerberus");

  Assets::Manager::the().load<Image>("sf.ktx2");
  cerberus.add_component<Component::Material>("main_geometry");
  cerberus.add_component<Component::Mesh>("cerberus/cerberus.gltf");
  cerberus.get_component<Component::Transform>().scale = { 0.05, 0.05, 0.05 };

  {
    ZoneScopedN("Create tools");
    smoother = std::make_unique<FrametimeSmoother>();
    plotter = std::make_unique<FrameTimePlotter>();
  }
}

AppLayer::~AppLayer() = default;

auto
AppLayer::on_destroy() -> void
{
}

auto
AppLayer::on_event(Event& event) -> bool
{
  if (file_browser.on_event(event)) {
    return true;
  }

  if (camera->on_event(event)) {
    return true;
  }
  return active_scene->on_event(event);
}

auto
AppLayer::on_interface() -> void
{
  static constexpr auto window = [](const std::string_view name, auto&& fn) {
    if (ImGui::Begin(name.data())) {
      fn();
      ImGui::End();
    }
  };

  active_scene->on_interface();

  window("Controls", [&, this]() {
    ImGui::SliderFloat("Rotation Speed", &rotation_speed, 0.1f, 150.0f);
  });

  static bool choice = false;
  int flags = 0;
  if (choice) {
    static constexpr auto default_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBackground;
    flags = default_flags;
  }

  if (debounce_toggle(KeyCode::F7, 0.2F)) {
    choice = !choice;
  }

  file_browser.on_interface();

  if (ImGui::Begin("Renderer Output", nullptr, flags)) {
    ZoneScopedN("Renderer Output");

    const auto available = ImGui::GetContentRegionAvail();
    auto& image = renderer->get_output_image();
    float render_aspect = image.get_aspect_ratio();
    float region_aspect = available.x / available.y;

    glm::vec2 image_size;
    if (region_aspect > render_aspect) {
      image_size.y = available.y;
      image_size.x = render_aspect * image_size.y;
    } else {
      image_size.x = available.x;
      image_size.y = image_size.x / render_aspect;
    }
    ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor.x + (available.x - image_size.x) * 0.5f,
                               cursor.y + (available.y - image_size.y) * 0.5f));
    ImGui::SetNextItemAllowOverlap(); // Optional but helps avoid warning
    if (auto texture_id = image.get_texture_id<ImTextureID>()) {
      ImGui::Image(*texture_id, ImVec2(image_size.x, image_size.y));
    }

    ImGui::SetCursorScreenPos(ImGui::GetItemRectMin());
    ImGui::InvisibleButton("ViewportInputRegion",
                           ImVec2(image_size.x, image_size.y),
                           ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonRight |
                             ImGuiButtonFlags_MouseButtonMiddle);

    const auto image_top_left =
      glm::vec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y);
    const auto image_bottom_right =
      glm::vec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y);

    viewport_bounds.min = glm::vec2(image_top_left.x, image_top_left.y);
    viewport_bounds.max = glm::vec2(image_bottom_right.x, image_bottom_right.y);

    const bool hovered = ImGui::IsItemHovered();

    if (const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        hovered && clicked) {
      const ImVec2 mouse = ImGui::GetMousePos();
      const glm::vec2 mouse_screen(mouse.x, mouse.y);
      const glm::vec2 relative_mouse_pos = mouse_screen - image_top_left;

      auto proj = camera->get_projection();
      auto&& [ray_origin, ray_dir] =
        generate_world_ray(relative_mouse_pos,
                           glm::vec2(image_size.x, image_size.y),
                           camera->get_position(),
                           camera->get_view(),
                           proj);

      on_ray_pick(ray_origin, ray_dir);
    }

    if (active_scene->selected_is_valid()) {
      const auto view = camera->get_view();
      glm::mat4 projection = camera->get_projection();

      ImGuizmo::SetOrthographic(false);

      // Draws inside the renderer output.
      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::SetRect(ImGui::GetWindowPos().x,
                        ImGui::GetWindowPos().y,
                        ImGui::GetWindowWidth(),
                        ImGui::GetWindowHeight());

      auto& transform = active_scene->get_registry().get<Component::Transform>(
        active_scene->get_selected_entity());
      auto model = transform.compute();

      if (ImGuizmo::Manipulate(glm::value_ptr(view),
                               glm::value_ptr(projection),
                               ImGuizmo::OPERATION::TRANSLATE,
                               ImGuizmo::LOCAL,
                               glm::value_ptr(model))) {
        glm::vec3 skew{};
        glm::vec3 translation{};
        glm::vec3 scale{};
        glm::vec4 perspective{};
        glm::quat rotation{};

        if (glm::decompose(
              model, scale, rotation, translation, skew, perspective)) {
          constexpr float scale_min = 0.001f;
          scale.x = glm::max(glm::abs(scale.x), scale_min) * glm::sign(scale.x);
          scale.y = glm::max(glm::abs(scale.y), scale_min) * glm::sign(scale.y);
          scale.z = glm::max(glm::abs(scale.z), scale_min) * glm::sign(scale.z);

          if (!glm::any(glm::isnan(rotation)) &&
              glm::length2(rotation) > 0.0001f)
            rotation = glm::normalize(rotation);
          else
            rotation = glm::quat_identity<float, glm::defaultp>();

          transform.position = translation;
          transform.rotation = rotation;
          transform.scale = scale;
        }
      }
    }

    ImGui::End();
  }

  if (const auto* image = renderer->get_shadow_image();
      image && ImGui::Begin("Shadow Output", nullptr, flags)) {
    ZoneScopedN("Shadow Output");
    auto size = ImGui::GetWindowSize();
    auto texture_id = image->get_texture_id<ImTextureID>();
    if (texture_id) {
      ImGui::Image(*texture_id, size);
    }
    ImGui::End();
  }

  if (const auto* image = renderer->get_point_lights_image();
      image && ImGui::Begin("Point lights", nullptr, flags)) {
    ZoneScopedN("Point lights (GUI)");
    auto size = ImGui::GetWindowSize();
    auto texture_id = image->get_texture_id<ImTextureID>();
    if (texture_id) {
      ImGui::Image(*texture_id, size);
    }
    ImGui::End();
  }

  if (ImGui::Begin("Performance Metrics")) {
    ZoneScopedN("Performance Metrics");
    plotter->plot();
    auto& io = ImGui::GetIO();
    ImGui::Text("ImGui FPS: %.2f", io.Framerate);
    ImGui::Text("Smoothed FPS: %.2f", 1.0 / smoother->get_smoothed());
    ImGui::Text("Smoothed Frame Time: %.2f ms",
                smoother->get_smoothed() * 1000.0);
    ImGui::End();
  }

  if (ImGui::Begin("GPU Timers")) {
#define PERFORMANCE
#ifdef PERFORMANCE
    ZoneScopedN("GPU Timers");
    const auto& command_buffer = renderer->get_command_buffer();
    const auto& compute_command_buffer = renderer->get_compute_command_buffer();

    // This should be from the swapchain index technically
    auto raster_timings =
      command_buffer.resolve_timers(renderer->get_frame_index());
    auto compute_timings =
      compute_command_buffer.resolve_timers(renderer->get_frame_index());

    ImGui::BeginTable("GPU Timings", 3);
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Duration (ms)");
    ImGui::TableSetupColumn("Command Buffer");
    ImGui::TableHeadersRow();

    for (const auto& section : raster_timings) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", section.name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", section.duration_ms);
      ImGui::TableNextColumn();
      ImGui::Text("Raster");
    }

    for (const auto& section : compute_timings) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", section.name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", section.duration_ms);
      ImGui::TableNextColumn();
      ImGui::Text("Compute");
    }

    ImGui::EndTable();
#endif
    ImGui::End();
  }
}

auto
AppLayer::on_update(const double ts) -> void
{
  ZoneScopedN("On update");
  {
    ZoneScopedN("Add samples to smoothers");
    smoother->add_sample(ts);
    plotter->add_sample(ts);
  }
  file_browser.on_update(ts);
  camera->on_update(ts);
  active_scene->on_update(ts);

  static float angle_deg = 0.f;
  angle_deg += static_cast<float>(ts) * rotation_speed;
  angle_deg = fmod(angle_deg, 360.f);

  active_scene->each<const CubeComponent, Component::Transform>(
    [&](auto, const auto&, Component::Transform& transform) {
      transform.rotation =
        glm::angleAxis(glm::radians(angle_deg), glm::vec3(0.f, 1.f, 0.f));
    });
}

auto
AppLayer::on_render(Renderer&) -> void
{
  ZoneScopedN("App on_render");
  static CameraMatrices camera_data{};
  if (const bool found = get_camera_matrices(camera_data); !found) {
    camera_data.projection = camera->get_projection();
    camera_data.inverse_projection = camera->get_inverse_projection();
    camera_data.view = camera->get_view();
  }

  renderer->begin_frame({
    .projection = camera_data.projection,
    .inverse_projection = camera_data.inverse_projection,
    .view = camera_data.view,
  });

  active_scene->on_render(*renderer);

  static constexpr float line_width = 1.2f;
  static constexpr float line_length = 50.f;

  renderer->submit_lines({ 0.f, 0.f, 0.f },
                         { line_length, 0.f, 0.f },
                         line_width,
                         { 1.f, 0.f, 0.f, 1.f });
  renderer->submit_lines({ 0.f, 0.f, 0.f },
                         { 0.f, line_length, 0.f },
                         line_width,
                         { 0.f, 1.f, 0.f, 1.f });
  renderer->submit_lines({ 0.f, 0.f, 0.f },
                         { 0.f, 0.f, line_length },
                         line_width,
                         { 0.f, 0.f, 1.f, 1.f });
}

auto
AppLayer::on_resize(std::uint32_t w, std::uint32_t h) -> void
{
  camera->resize(w, h);
  // Update frustums, subrenderers, etc.
  renderer->update_camera(*camera);
  // Update the scene entity camera (for now a duplicate of the editor camera)
  active_scene->on_resize(*camera);

  bounds.x = static_cast<float>(w);
  bounds.y = static_cast<float>(h);
}

auto
AppLayer::on_initialise(const InitialisationParameters& params) -> void
{
  auto&& [w, h] = params.window.framebuffer_size();
  camera = std::make_unique<EditorCamera>(
    90.0F, static_cast<float>(w) / static_cast<float>(h), 0.1F, 1000.0F);
  renderer->update_camera(*camera);

  active_scene->on_resize(*camera);

  auto& pls = renderer->get_point_light_system();
  generate_scene(pls);
}

auto
AppLayer::get_camera_matrices(CameraMatrices& out) const -> bool
{
  static constexpr auto prefer_editor = true;

  if (prefer_editor) {
    return false;
  }

  const auto view = active_scene->view<Component::Camera>();
  // For now, we get the first camera, whichever that is.
  auto entity = view.front();
  if (active_scene->get_registry().valid(entity)) {
    const auto& cam = view.get<Component::Camera>(entity);
    const auto& tr =
      active_scene->get_registry().get<Component::Transform>(entity);
    out.projection = cam.projection;
    out.inverse_projection = cam.inverse_projection,
    out.view = glm::inverse(tr.compute());
    return true;
  }
  return false;
}

auto
AppLayer::generate_scene(PointLightSystem& pls) -> void
{
  {
    auto sponza = active_scene->create_entity("Sponza");
    auto& mesh = sponza.add_component<Component::Mesh>(
      //"main_sponza/NewSponza_Main_glTF_003.gltf");
      "sponza/sponza.gltf");
    mesh.casts_shadows = true;
    auto& transform = sponza.get_component<Component::Transform>();
    transform.scale = glm::vec3(0.01f, 0.01f, 0.01f);
  }

  {
    auto entity = active_scene->create_entity("Ground");
    auto& transform = entity.get_component<Component::Transform>();
    auto& cube_mesh =
      entity.add_component<Component::Mesh>(Assets::builtin_cube());
    cube_mesh.casts_shadows = true;

    transform.position = glm::vec3(6.0f, -1.0f, 6.0f);
    transform.scale = glm::vec3(24.0f, 1.0f, 24.0f);
  }

  auto cube_parent = active_scene->create_entity("CubesParent");
  for (int z = 0; z < 3; ++z) {
    for (int x = 0; x < 4; ++x) {
      auto entity =
        active_scene->create_entity(std::format("Cube_{}_{}", x, z));
      auto& cube_mesh =
        entity.add_component<Component::Mesh>(Assets::builtin_cube());
      cube_mesh.casts_shadows = true;
      entity.add_component<CubeComponent>();
      entity.set_parent(cube_parent);
      auto& transform = entity.get_component<Component::Transform>();
      transform.position = glm::vec3(x * 4.0f, 1.0f, z * 4.0f);

      if (z + x == 3) {
        auto& mat = entity.add_component<Component::Material>("main_geometry");
        auto& mat_data = mat.material.get()->get_material_data();
        mat_data.emissive_strength = 20.0F;
        mat_data.emissive_color = Utils::Random::random_single_channel_colour();
      }
    }
  }

  auto all_lights = active_scene->create_entity("AllLightsParent");
  for (auto i : std::views::iota(0, 2048)) {
    auto point_light =
      active_scene->create_entity(std::format("PointLight_{}", i));
    auto& light = point_light.add_component<Component::PointLight>();
    auto& transform = point_light.get_component<Component::Transform>();
    transform.position = glm::vec3(Utils::Random::random_float(-12.0f, 12.0f),
                                   Utils::Random::random_float(-12.0f, 12.0f),
                                   Utils::Random::random_float(-12.0f, 12.0f));
    transform.scale = 0.1F * glm::vec3(1.0f, 1.0f, 1.0f);

    light.color = Utils::Random::random_single_channel_colour();
    light.intensity = Utils::Random::random_float(0.5f, 7.0f);
    light.radius = Utils::Random::random_float(1.0f, 10.0f);
    point_light.set_parent(all_lights);
    pls.add_light(entt::to_integral(point_light.raw()),
                  point_light.get_component<Component::Transform>().position,
                  point_light.get_component<Component::PointLight>());
  }
}

auto
AppLayer::on_ray_pick(const glm::vec3& origin, const glm::vec3& direction)
  -> void
{
  if (!active_scene)
    return;

  float closest_distance = std::numeric_limits<float>::max();
  entt::entity closest_entity = entt::null;

  active_scene->each<const CubeComponent, const Component::Transform>(
    [&](const entt::entity entity,
        const CubeComponent&,
        const Component::Transform& transform) {
      const glm::mat4 model = transform.compute();
      const glm::mat4 inv_model = glm::inverse(model);

      const auto ray_origin_local =
        glm::vec3(inv_model * glm::vec4(origin, 1.0f));
      const auto ray_dir_local =
        glm::normalize(glm::vec3(inv_model * glm::vec4(direction, 0.0f)));

      constexpr glm::vec3 aabb_min(-0.5f), aabb_max(0.5f);
      float tmin = -std::numeric_limits<float>::max();
      float tmax = std::numeric_limits<float>::max();

      for (int i = 0; i < 3; ++i) {
        const float dir_component = ray_dir_local[i];
        const float orig_component = ray_origin_local[i];

        if (std::abs(dir_component) < 1e-6f) {
          if (orig_component < aabb_min[i] || orig_component > aabb_max[i])
            return; // Parallel and outside slab
          continue;
        }

        const float inv_d = 1.0f / dir_component;
        float t0 = (aabb_min[i] - orig_component) * inv_d;
        float t1 = (aabb_max[i] - orig_component) * inv_d;

        if (inv_d < 0.0f)
          std::swap(t0, t1);

        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin)
          return;
      }

      if (tmin < closest_distance && tmax >= 0.0f) {
        closest_distance = tmin;
        closest_entity = entity;
      }
    });

  if (closest_entity != entt::null) {
    const ReadonlyEntity entity{ closest_entity, active_scene.get() };
    if (auto* tag = entity.try_get<const Component::Tag>()) {
      Logger::log_info("{}", tag->name);
      active_scene->set_selected_entity(closest_entity);
    }
  }
}
