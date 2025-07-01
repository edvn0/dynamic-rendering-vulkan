#pragma once

#include <algorithm>
#include <deque>
#include <filesystem>
#include <imgui.h>
#include <string>
#include <vector>

#include <dynamic_rendering/assets/handle.hpp>
#include <dynamic_rendering/core/event_system.hpp>
#include <dynamic_rendering/core/forward.hpp>

namespace fs = std::filesystem;

class FileBrowser
{
private:
  struct FileEntry
  {
    std::string name;
    std::string full_path;
    bool is_directory;
    size_t file_size;
    std::string extension;

    FileEntry(const std::string&, const std::string&, bool, size_t = 0);
  };

  std::string current_path;
  std::vector<FileEntry> current_entries;
  std::deque<std::string> history;
  std::optional<std::int32_t> queued_navigation;
  int history_index{ -1 };

  // UI state
  int selected_index{ -1 };
  bool show_hidden_files{ false };

  // Preview system
  std::string preview_file_path;
  Assets::Handle<Image> preview_image;
  bool preview_loading{ false };
  ImTextureID preview_texture_id;

  // Constants
  static inline const std::vector<std::string> IMAGE_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".webp"
  };

public:
  FileBrowser(const std::filesystem::path& initial_path)
    : current_path(std::filesystem::canonical(initial_path).string())
  {
    const auto canonical_path =
      std::filesystem::canonical(initial_path).string();
    history.push_back(canonical_path);
    history_index = 0;
    navigate_to(canonical_path);
  }

  void on_update(std::floating_point auto) { update_preview(); }
  auto on_event(Event&) -> bool;
  void on_interface();

private:
  void render_navigation_bar();

  void navigate_to(const std::string&);
  void refresh_entries();
  void render_file_list();
  void render_preview_panel();
  void render_image_preview(const std::string&);

  void activate_entry(int index);
  void update_selection();
  void load_image_preview(const std::string& image_path);

  void clear_preview();
  void update_preview();

  bool can_go_back() const;
  bool can_go_forward() const;
  void go_back();
  void go_forward();
  void go_up();

  bool is_image_file(const std::string& extension) const;

  std::string format_file_size(size_t bytes) const;
};