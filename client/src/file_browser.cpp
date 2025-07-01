#include "file_browser.hpp"

#include <dynamic_rendering/assets/manager.hpp>

#include <imgui_internal.h>

static constexpr float max_size = 200.0f;
static constexpr auto max_size_uint = static_cast<std::uint32_t>(max_size);

FileBrowser::FileEntry::FileEntry(const std::string& n,
                                  const std::string& path,
                                  bool dir,
                                  size_t size)
  : name(n)
  , full_path(path)
  , is_directory(dir)
  , file_size(size)
{
  if (!dir) {
    auto pos = name.rfind('.');
    if (pos != std::string::npos) {
      extension = name.substr(pos);
      std::transform(
        extension.begin(), extension.end(), extension.begin(), ::tolower);
    }
  }
}

void
FileBrowser::on_interface()
{
  ImGui::Begin("File Browser");

  render_navigation_bar();
  ImGui::Separator();

  // Main content area
  ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

  // Split view: file list on left, preview on right
  ImGui::Columns(2, "BrowserColumns", true);

  render_file_list();

  ImGui::NextColumn();

  render_preview_panel();

  ImGui::Columns(1);
  ImGui::EndChild();

  ImGui::End();
}

void
FileBrowser::render_image_preview(const std::string& image_path)
{
  if (preview_file_path != image_path) {
    load_image_preview(image_path);
  }

  if (preview_loading) {
    ImGui::Text("Loading preview...");
  } else if (preview_image.is_valid() && preview_texture_id) {
    ImGui::Text("Preview:");

    // Calculate preview size while maintaining aspect ratio
    auto* real_image = preview_image.get();
    float width = static_cast<float>(real_image->width());
    float height = static_cast<float>(real_image->height());

    float scale = std::min(max_size / width, max_size / height);
    ImVec2 preview_size(width * scale, height * scale);

    ImGui::Image(preview_texture_id, preview_size);

    ImGui::Text("Dimensions: %.0fx%.0f", width * scale, height * scale);
  } else if (!preview_file_path.empty()) {
    ImGui::Text("Failed to load preview");
  }
}

void
FileBrowser::render_navigation_bar()
{
  // Back button
  bool can_back = can_go_back();
  if (!can_back)
    ImGui::BeginDisabled();
  if (ImGui::Button("◀")) {
    go_back();
  }
  if (!can_back)
    ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Go Back");
  }

  ImGui::SameLine();

  // Forward button
  bool can_forward = can_go_forward();
  if (!can_forward)
    ImGui::BeginDisabled();
  if (ImGui::Button("▶")) {
    go_forward();
  }
  if (!can_forward)
    ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Go Forward");
  }

  ImGui::SameLine();

  // Up button
  if (ImGui::Button("⬆️")) {
    go_up();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Go Up");
  }

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // Path display
  ImGui::Text("Path: %s", current_path.c_str());

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // Options
  if (ImGui::Checkbox("Show Hidden", &show_hidden_files)) {
    refresh_entries();
  }

  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    refresh_entries();
  }
}

void
FileBrowser::load_image_preview(const std::string& image_path)
{
  if (preview_file_path == image_path && preview_image.is_valid()) {
    return; // Already loaded
  }

  clear_preview();
  preview_file_path = image_path;
  preview_loading = true;

  SampledTextureImageConfiguration config;
  config.extent = { max_size_uint, max_size_uint };

  // Use your asset manager to load the image
  preview_image =
    Assets::Manager::the().load<Image, SampledTextureImageConfiguration>(
      image_path, config);

  if (preview_image.is_valid()) {
    // Convert to ImGui texture - this depends on your graphics backend
    // You'll need to implement get_imgui_texture_id() in your Image class
    preview_texture_id =
      preview_image.get()->get_texture_id<ImTextureID>().value_or(0);
  } else {
    Logger::log_error("Failed to load image preview: {}", image_path);
    preview_texture_id = 0;
  }

  preview_loading = false;
}

void
FileBrowser::navigate_to(const std::string& path)
{
  try {
    if (!fs::exists(path) || !fs::is_directory(path)) {
      return;
    }

    // Add to history if this is a new navigation
    if (history_index == -1 || history[history_index] != path) {
      // Remove everything after current position
      history.erase(history.begin() + history_index + 1, history.end());
      history.push_back(path);
      history_index = static_cast<std::int32_t>(history.size()) - 1;

      // Limit history size
      if (history.size() > 50) {
        history.pop_front();
        history_index--;
      }
    }

    current_path = fs::canonical(path).string();
    refresh_entries();
    selected_index = -1;
    clear_preview();

  } catch (const fs::filesystem_error& e) {
    // Handle error - could show in status bar
    Logger::log_error("Failed to navigate: {}", e.what());
  }
}

void
FileBrowser::refresh_entries()
{
  current_entries.clear();

  try {
    // Add parent directory entry if not at root
    if (current_path != fs::current_path().root_path()) {
      current_entries.emplace_back(
        "..", fs::path(current_path).parent_path().string(), true);
    }

    // Collect all entries
    std::vector<FileEntry> directories;
    std::vector<FileEntry> files;

    for (const auto& entry : fs::directory_iterator(current_path)) {
      std::string filename = entry.path().filename().string();

      // Skip hidden files if not showing them
      if (!show_hidden_files && filename[0] == '.') {
        continue;
      }

      if (entry.is_directory()) {
        directories.emplace_back(filename, entry.path().string(), true);
      } else {
        size_t file_size = 0;
        try {
          file_size = fs::file_size(entry);
        } catch (...) {
        }

        files.emplace_back(filename, entry.path().string(), false, file_size);
      }
    }

    // Sort directories and files separately
    auto sort_pred = [](const FileEntry& a, const FileEntry& b) {
      return a.name < b.name;
    };

    std::sort(directories.begin(), directories.end(), sort_pred);
    std::sort(files.begin(), files.end(), sort_pred);

    // Add to current_entries
    current_entries.insert(
      current_entries.end(), directories.begin(), directories.end());
    current_entries.insert(current_entries.end(), files.begin(), files.end());

  } catch (const fs::filesystem_error& e) {
    Logger::log_error("Failed to refresh entries: {}", e.what());
    // Handle error
  }
}

void
FileBrowser::render_file_list()
{
  ImGui::BeginChild("FileList", ImVec2(0, 0), true);

  ImGuiListClipper clipper;
  const auto size = static_cast<int>(current_entries.size());
  clipper.Begin(size);

  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
      const auto& entry = current_entries[i];

      // Selection handling
      bool is_selected = (selected_index == i);
      ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;

      // Icon and name
      std::string display_name =
        entry.is_directory ? ("📁 " + entry.name) : ("📄 " + entry.name);

      if (ImGui::Selectable(display_name.c_str(), is_selected, flags)) {
        selected_index = i;
        update_selection();

        // Double-click to activate
        if (ImGui::IsMouseDoubleClicked(0)) {
          queued_navigation = i;
        }
      }

      const auto valid_entry = !entry.is_directory && entry.file_size > 0;

      // Drag drop for files, not directories and not empty files
      if (valid_entry &&
          ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        std::string payload_type = "FILE_BROWSER_ENTRY";

        auto full_path = fs::canonical(entry.full_path).string();
        ImGui::SetDragDropPayload(
          payload_type.c_str(), full_path.c_str(), full_path.size() + 1);

        ImGui::Text("%s", display_name.c_str());

        ImGui::EndDragDropSource();
      }

      if (valid_entry) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", format_file_size(entry.file_size).c_str());
      }
    }
  }

  clipper.End();
  ImGui::EndChild();

  if (queued_navigation.has_value()) {
    activate_entry(queued_navigation.value());
    queued_navigation.reset();
  }
}

void
FileBrowser::render_preview_panel()
{
  ImGui::BeginChild("Preview", ImVec2(0, 0), true);

  if (selected_index >= 0 && selected_index < current_entries.size()) {
    const auto& entry = current_entries[selected_index];

    ImGui::Text("Selected: %s", entry.name.c_str());

    if (entry.is_directory) {
      ImGui::Text("Type: Directory");
    } else {
      ImGui::Text("Type: File");
      ImGui::Text("Size: %s", format_file_size(entry.file_size).c_str());
      ImGui::Text("Extension: %s", entry.extension.c_str());

      // Show image preview if applicable
      if (is_image_file(entry.extension)) {
        ImGui::Separator();
        render_image_preview(entry.full_path);
      }
    }
  } else {
    ImGui::Text("No selection");
  }

  ImGui::EndChild();
}

auto
FileBrowser::on_event(Event& event) -> bool
{
  EventDispatcher dispatcher(event);
  dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
    if (e.key == KeyCode::Down) {
      selected_index =
        std::min(selected_index + 1, (int)current_entries.size() - 1);
      update_selection();
      return true;
    } else if (e.key == KeyCode::Up) {
      selected_index = std::max(selected_index - 1, 0);
      return true;
    } else if (e.key == KeyCode::Enter) {
      if (selected_index >= 0) {
        activate_entry(selected_index);
        return true;
      }
    }
    return false;
  });

  // Back, forward mouse buttons
  dispatcher.dispatch<MouseButtonPressedEvent>(
    [this](MouseButtonPressedEvent& e) {
      const auto is_back = MouseCode::Back == e.button;
      const auto is_forward = MouseCode::Forward == e.button;

      if (is_back && can_go_back()) {
        go_back();
        return true;
      } else if (is_forward && can_go_forward()) {
        go_forward();
        return true;
      }

      return false;
    });

  return event.handled;
}

void
FileBrowser::activate_entry(int index)
{
  if (index < 0 || index >= current_entries.size())
    return;

  const auto& entry = current_entries[index];
  if (entry.is_directory) {
    navigate_to(entry.full_path);
  }
  // For files, could open them or show more details
}

void
FileBrowser::update_selection()
{
  // Update preview when selection changes
  if (selected_index >= 0 && selected_index < current_entries.size()) {
    const auto& entry = current_entries[selected_index];
    if (!entry.is_directory && is_image_file(entry.extension)) {
      load_image_preview(entry.full_path);
    } else {
      clear_preview();
    }
  }
}

void
FileBrowser::clear_preview()
{
  preview_file_path.clear();
  preview_image.reset();
  preview_texture_id = 0;
  preview_loading = false;
}

void
FileBrowser::update_preview()
{
  // Handle any async loading if needed
  // For now, preview loading is synchronous
}

bool
FileBrowser::can_go_back() const
{
  return history_index > 0;
}

bool
FileBrowser::can_go_forward() const
{
  return history_index >= 0 && history_index < (int)history.size() - 1;
}

void
FileBrowser::go_back()
{
  if (can_go_back()) {
    history_index--;
    current_path = history[history_index];
    refresh_entries();
    selected_index = -1;
    clear_preview();
  }
}

void
FileBrowser::go_forward()
{
  if (can_go_forward()) {
    history_index++;
    current_path = history[history_index];
    refresh_entries();
    selected_index = -1;
    clear_preview();
  }
}

void
FileBrowser::go_up()
{
  fs::path parent = fs::path(current_path).parent_path();
  if (parent != current_path) {
    navigate_to(parent.string());
  }
}

bool
FileBrowser::is_image_file(const std::string& extension) const
{
  return std::find(IMAGE_EXTENSIONS.begin(),
                   IMAGE_EXTENSIONS.end(),
                   extension) != IMAGE_EXTENSIONS.end();
}

std::string
FileBrowser::format_file_size(size_t bytes) const
{
  const char* units[] = { "B", "KB", "MB", "GB", "TB" };
  int unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024.0 && unit_index < 4) {
    size /= 1024.0;
    unit_index++;
  }

  char buffer[32];
  if (unit_index == 0) {
    snprintf(buffer, sizeof(buffer), "%.0f %s", size, units[unit_index]);
  } else {
    snprintf(buffer, sizeof(buffer), "%.1f %s", size, units[unit_index]);
  }

  return std::string(buffer);
}