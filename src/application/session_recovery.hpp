#pragma once

#include "editor/text_buffer.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace note::application {

struct SessionDocument {
    QString path;
    QString recoveryKey;
    std::uint64_t cursorOffset{};
};

struct SessionState {
    std::vector<SessionDocument> documents;
    int activeIndex{-1};
    bool previousRunClean{true};
    QString error;
};

struct RecoveryDocument {
    QString key;
    QString sourcePath;
    const editor::TextBuffer* buffer{};
    std::uint64_t revision{};
};

struct RecoveredSnapshot {
    QString key;
    QString sourcePath;
    std::optional<editor::TextSnapshot> contents;
    QString error;
};

class SessionRecoveryService final : public QObject {
    Q_OBJECT
public:
    using SessionProvider = std::function<SessionState()>;
    using RecoveryProvider = std::function<std::vector<RecoveryDocument>()>;

    explicit SessionRecoveryService(QString dataDirectory, QObject* parent = nullptr);
    [[nodiscard]] SessionState start();
    void setProviders(SessionProvider sessionProvider,
                      RecoveryProvider recoveryProvider);
    void clearProviders();
    void scheduleSessionSave();
    void scheduleRecoverySnapshot();
    void flushNow();
    void flushRecoverySnapshotsNow();
    void finishNormally(const SessionState& state,
                        bool preserveRecoveryForNextStartup = false);
    [[nodiscard]] std::vector<RecoveredSnapshot> recoveries() const;
    void removeRecovery(const QString& key);
    [[nodiscard]] QString errorString() const;

signals:
    void persistenceError(const QString& message);

private:
    void persistAndSnapshot();
    void snapshotModifiedDocuments();
    bool saveSession(const SessionState& state, bool clean) const;
    bool saveSnapshot(const RecoveryDocument& document);
    [[nodiscard]] QString snapshotPath(const QString& key) const;
    void reportError(const QString& message);

    QString dataDirectory_;
    QString sessionPath_;
    QString error_;
    SessionProvider sessionProvider_;
    RecoveryProvider recoveryProvider_;
    QTimer periodicTimer_;
    QTimer debounceTimer_;
    std::map<QString, std::uint64_t> lastSnapshotRevision_;
};

} // namespace note::application
