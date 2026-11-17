#include "platform/file_monitor.hpp"
#include "platform/path_resolver.hpp"

#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include <set>

namespace note::platform {
namespace {

QString absolutePath(const std::filesystem::path& path) {
    return QDir::cleanPath(QFileInfo(pathToQString(path)).absoluteFilePath());
}

} // namespace

FileMonitor::FileMonitor(ChangedCallback callback) : callback_(std::move(callback)) {
    QObject::connect(&watcher_, &QFileSystemWatcher::fileChanged, &watcher_,
                     [this](const QString& path) {
        notifyFile(path);
        QTimer::singleShot(0, &watcher_, [this] { rebuildWatches(); });
    });
    QObject::connect(&watcher_, &QFileSystemWatcher::directoryChanged, &watcher_,
                     [this](const QString& path) {
        notifyDirectory(path);
        QTimer::singleShot(0, &watcher_, [this] { rebuildWatches(); });
    });
}

void FileMonitor::monitor(documents::DocumentId id, const std::filesystem::path& path) {
    paths_[id] = path;
    rebuildWatches();
}

void FileMonitor::remove(documents::DocumentId id) {
    if (paths_.erase(id) != 0) rebuildWatches();
}

void FileMonitor::clear() {
    paths_.clear();
    rebuildWatches();
}

void FileMonitor::refresh() { rebuildWatches(); }

void FileMonitor::rebuildWatches() {
    const auto oldFiles = watcher_.files();
    const auto oldDirectories = watcher_.directories();
    if (!oldFiles.isEmpty()) watcher_.removePaths(oldFiles);
    if (!oldDirectories.isEmpty()) watcher_.removePaths(oldDirectories);

    std::set<QString> files;
    std::set<QString> directories;
    for (const auto& [id, path] : paths_) {
        (void)id;
        const auto file = absolutePath(path);
        const QFileInfo info(file);
        const auto directory = info.absolutePath();
        if (info.exists()) files.insert(file);
        if (QFileInfo::exists(directory)) directories.insert(directory);
    }
    for (const auto& file : files) (void)watcher_.addPath(file);
    for (const auto& directory : directories) (void)watcher_.addPath(directory);
}

void FileMonitor::notifyFile(const QString& changedPath) {
    const auto changed = QDir::cleanPath(QFileInfo(changedPath).absoluteFilePath());
    for (const auto& [id, path] : paths_)
        if (absolutePath(path) == changed && callback_) callback_(id);
}

void FileMonitor::notifyDirectory(const QString& changedPath) {
    const auto changed = QDir::cleanPath(QFileInfo(changedPath).absoluteFilePath());
    for (const auto& [id, path] : paths_)
        if (QFileInfo(absolutePath(path)).absolutePath() == changed && callback_) callback_(id);
}

} // namespace note::platform
