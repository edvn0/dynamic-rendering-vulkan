#include "core/asset_file_watcher.hpp"

#include "core/fs.hpp"

#include <chrono>
#include <shared_mutex>

#ifdef WIN32
#include <windows.h>
#endif

AssetFileWatcher::AssetFileWatcher()
{
  file_watcher = std::make_unique<efsw::FileWatcher>();
  file_watcher->watch();
}

AssetFileWatcher::~AssetFileWatcher()
{
  stop();
}

auto
AssetFileWatcher::start_monitoring(
  const std::span<const std::filesystem::path> directories) -> void
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
  watch_directories.clear();

  const auto& root = assets_path();
  auto id = file_watcher->addWatch(root.string(), this, true, options);
  watch_directories[id] = root.string();
  for (const auto& dir : directories) {
    auto path = root / dir;
    if (!std::filesystem::exists(path)) {
      std::cerr << "Directory does not exist: " << path.string() << std::endl;
      continue;
    }
    auto computed_watch =
      file_watcher->addWatch(path.string(), this, true, options);
    watch_directories[computed_watch] = path.string();
  }
}

auto
AssetFileWatcher::stop() -> void
{
  if (file_watcher) {
    std::ranges::for_each(watch_directories, [this](const auto& pair) {
      file_watcher->removeWatch(pair.first);
    });
    file_watcher.reset();
  }
  std::unique_lock lock(dirty_mutex);
  pending_changes.clear();
  watch_directories.clear();
}

auto
AssetFileWatcher::collect_dirty() -> string_hash_set
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
AssetFileWatcher::handleFileAction(efsw::WatchID /*watchid*/,
                                   const std::string& directory,
                                   const std::string& filename,
                                   efsw::Action action,
                                   std::string /*old_filename*/)
{
  if (action != efsw::Actions::Modified) {
    return;
  }

  std::unique_lock lock(dirty_mutex);
  auto full_path = std::filesystem::path{ directory } / filename;
  pending_changes[full_path.string()] = std::chrono::steady_clock::now();
}
