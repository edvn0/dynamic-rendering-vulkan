#pragma once

#include "core/util.hpp"

#include <array>
#include <chrono>
#include <efsw/efsw.hpp>
#include <filesystem>
#include <shared_mutex>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

class AssetFileWatcher : public efsw::FileWatchListener
{
public:
  AssetFileWatcher();
  ~AssetFileWatcher();

  auto start_monitoring(const std::span<const std::filesystem::path>) -> void;
  auto start_monitoring() -> void
  {
    std::array<std::filesystem::path, 0> empty{};
    start_monitoring(empty);
  }
  auto stop() -> void;
  auto collect_dirty() -> string_hash_set;
  auto handleFileAction(efsw::WatchID,
                        const std::string&,
                        const std::string& filename,
                        efsw::Action action,
                        std::string) -> void override;

private:
  std::unique_ptr<efsw::FileWatcher> file_watcher;
  string_hash_map<std::chrono::steady_clock::time_point> pending_changes;
  std::unordered_map<efsw::WatchID, std::string> watch_directories;
  std::shared_mutex dirty_mutex;
  std::atomic_bool dirty_flag{ false };
  static constexpr int debounce_delay_ms = 100;
};
