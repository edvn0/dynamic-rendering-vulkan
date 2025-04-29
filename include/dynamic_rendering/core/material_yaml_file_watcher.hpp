#pragma once

#include "core/util.hpp"

#include <chrono>
#include <efsw/efsw.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_set>

class MaterialYAMLFileWatcher : public efsw::FileWatchListener
{
public:
  MaterialYAMLFileWatcher();
  ~MaterialYAMLFileWatcher();

  auto start_monitoring(const std::filesystem::path& directory) -> void;
  auto stop() -> void;
  auto collect_dirty() -> string_hash_set;

  void handleFileAction(efsw::WatchID watchid,
                        const std::string& dir,
                        const std::string& filename,
                        efsw::Action action,
                        std::string old_filename = "") override;

private:
  std::unique_ptr<efsw::FileWatcher> file_watcher;
  efsw::WatchID watch_id{};

  std::shared_mutex dirty_mutex;

  string_hash_map<std::chrono::steady_clock::time_point>
    pending_changes{}; // map of filename to last modified time

  static constexpr int debounce_delay_ms = 200;
};
