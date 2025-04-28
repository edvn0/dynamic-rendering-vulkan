#include "core/material_yaml_file_watcher.hpp"

#include <chrono>

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
  watch_id = file_watcher->addWatch(directory.string(), this, true);
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
  std::scoped_lock lock(dirty_mutex);
  using namespace std::chrono;

  string_hash_set ready;
  ready.reserve(pending_changes.size());

  const auto now = steady_clock::now();

  for (auto it = pending_changes.begin(); it != pending_changes.end();) {
    if (duration_cast<milliseconds>(now - it->second).count() >
        debounce_delay_ms) {
      ready.insert(it->first);
      it = pending_changes.erase(it);
    } else {
      ++it;
    }
  }

  return ready;
}
void
MaterialYAMLFileWatcher::handleFileAction(efsw::WatchID /*watchid*/,
                                          const std::string&,
                                          const std::string& filename,
                                          efsw::Action action,
                                          std::string /*old_filename*/)
{
  if (action == efsw::Actions::Modified) {
    std::scoped_lock lock(dirty_mutex);
    pending_changes[filename] = std::chrono::steady_clock::now();
  }
}
