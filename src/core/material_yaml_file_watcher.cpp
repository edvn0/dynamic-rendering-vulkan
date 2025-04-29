#include "core/material_yaml_file_watcher.hpp"

#include <chrono>
#include <shared_mutex>

#ifdef WIN32
#include <windows.h>
#endif

MaterialYAMLFileWatcher::MaterialYAMLFileWatcher()
{
  file_watcher = std::make_unique<efsw::FileWatcher>();
  file_watcher->watch();
}

MaterialYAMLFileWatcher::~MaterialYAMLFileWatcher()
{
  stop();
}

auto
MaterialYAMLFileWatcher::start_monitoring(
  const std::filesystem::path& directory) -> void
{
  auto options = std::vector<efsw::WatcherOption>{};
  static constexpr auto _256k = 256 * 1024;
#ifdef WIN32
  options.push_back(efsw::WatcherOption{
    efsw::Options::WinBufferSize,
    _256k,
  });
  options.push_back(efsw::WatcherOption{ efsw::Options::WinNotifyFilter,
                                         FILE_NOTIFY_CHANGE_LAST_WRITE });
#endif
  watch_id = file_watcher->addWatch(directory.string(), this, true, options);
}

auto
MaterialYAMLFileWatcher::stop() -> void
{
  if (file_watcher && watch_id != 0) {
    file_watcher->removeWatch(watch_id);
  }
}

auto
MaterialYAMLFileWatcher::collect_dirty() -> string_hash_set
{
  using namespace std::chrono;

  {
    std::shared_lock lock(dirty_mutex);
    if (pending_changes.empty()) {
      return {};
    }
  }

  std::unique_lock lock(dirty_mutex);

  string_hash_set ready;
  ready.reserve(pending_changes.size());

  auto n = steady_clock::now();
  erase_from_map_if(pending_changes,
                    [now = std::move(n),
                     &ready](const std::string& filename,
                             const std::chrono::steady_clock::time_point& t) {
                      if (duration_cast<milliseconds>(now - t).count() >
                          debounce_delay_ms) {
                        ready.insert(filename);
                        return true;
                      }
                      return false;
                    });

  return ready;
}

void
MaterialYAMLFileWatcher::handleFileAction(efsw::WatchID /*watchid*/,
                                          const std::string&,
                                          const std::string& filename,
                                          efsw::Action action,
                                          std::string /*old_filename*/)
{
  if (action != efsw::Actions::Modified) {
    return;
  }

  std::unique_lock lock(dirty_mutex);
  pending_changes[filename] = std::chrono::steady_clock::now();
}
