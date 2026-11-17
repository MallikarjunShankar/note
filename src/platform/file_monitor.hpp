#pragma once

#include "documents/document.hpp"

#include <QFileSystemWatcher>

#include <filesystem>
#include <functional>
#include <map>

namespace note::platform {

// Cross-platform change notifications backed by Qt's OS-specific filesystem
// watcher. The owner performs content verification and recovery decisions.
class FileMonitor final {
public:
    using ChangedCallback = std::function<void(documents::DocumentId)>;

    explicit FileMonitor(ChangedCallback callback);
    void monitor(documents::DocumentId id, const std::filesystem::path& path);
    void remove(documents::DocumentId id);
    void clear();
    void refresh();

private:
    void rebuildWatches();
    void notifyFile(const QString& changedPath);
    void notifyDirectory(const QString& changedPath);

    QFileSystemWatcher watcher_;
    std::map<documents::DocumentId, std::filesystem::path> paths_;
    ChangedCallback callback_;
};

} // namespace note::platform
