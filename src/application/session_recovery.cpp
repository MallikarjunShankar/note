#include "application/session_recovery.hpp"

#include <QCryptographicHash>
#include <QByteArrayView>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <limits>
#include <array>
#include <set>

namespace note::application {
namespace {

constexpr quint32 recoveryMagic = 0x4e4f5445; // NOTE
constexpr quint16 recoveryVersion = 1;
constexpr std::size_t streamChunkSize = 64 * 1024;

QString jsonString(const QJsonObject& object, const char* key) {
    const auto value = object.value(QLatin1String(key));
    return value.isString() ? value.toString() : QString{};
}

bool isSafeRecoveryKey(const QString& key) {
    if (key.isEmpty() || key.size() > 64) return false;
    for (const auto character : key) {
        const bool valid = (character >= QLatin1Char('a') &&
                            character <= QLatin1Char('z')) ||
                           (character >= QLatin1Char('A') &&
                            character <= QLatin1Char('Z')) ||
                           (character >= QLatin1Char('0') &&
                            character <= QLatin1Char('9')) ||
                           character == QLatin1Char('-') ||
                           character == QLatin1Char('_');
        if (!valid) return false;
    }
    return true;
}

bool validUtf8(const std::string& value) {
    std::size_t offset = 0;
    while (offset < value.size()) {
        const auto first = static_cast<unsigned char>(value[offset]);
        if (first < 0x80) {
            ++offset;
            continue;
        }
        std::size_t count = 0;
        unsigned char secondMin = 0x80;
        unsigned char secondMax = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) count = 2;
        else if (first == 0xe0) { count = 3; secondMin = 0xa0; }
        else if (first >= 0xe1 && first <= 0xec) count = 3;
        else if (first == 0xed) { count = 3; secondMax = 0x9f; }
        else if (first >= 0xee && first <= 0xef) count = 3;
        else if (first == 0xf0) { count = 4; secondMin = 0x90; }
        else if (first >= 0xf1 && first <= 0xf3) count = 4;
        else if (first == 0xf4) { count = 4; secondMax = 0x8f; }
        else return false;
        if (count > value.size() - offset) return false;
        const auto second = static_cast<unsigned char>(value[offset + 1]);
        if (second < secondMin || second > secondMax) return false;
        for (std::size_t index = 2; index < count; ++index) {
            const auto next = static_cast<unsigned char>(value[offset + index]);
            if (next < 0x80 || next > 0xbf) return false;
        }
        offset += count;
    }
    return true;
}

bool writeSessionFile(const QString& path, const SessionState& state, bool clean) {
    QJsonObject root;
    root["version"] = 1;
    root["cleanShutdown"] = clean;
    root["activeIndex"] = state.activeIndex;
    QJsonArray documents;
    for (const auto& document : state.documents) {
        QJsonObject item;
        item["path"] = document.path;
        item["recoveryKey"] = document.recoveryKey;
        item["cursorOffset"] = static_cast<qint64>(document.cursorOffset);
        documents.append(item);
    }
    root["documents"] = documents;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const auto payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    return file.write(payload) == payload.size() && file.commit();
}

} // namespace

SessionRecoveryService::SessionRecoveryService(QString dataDirectory, QObject* parent)
    : QObject(parent), dataDirectory_(std::move(dataDirectory)),
      sessionPath_(QDir(dataDirectory_).filePath(QStringLiteral("session.json"))) {
    periodicTimer_.setInterval(30000);
    debounceTimer_.setSingleShot(true);
    debounceTimer_.setInterval(900);
    connect(&periodicTimer_, &QTimer::timeout, this,
            [this] { persistAndSnapshot(); });
    connect(&debounceTimer_, &QTimer::timeout, this,
            [this] { persistAndSnapshot(); });
}

SessionState SessionRecoveryService::start() {
    QDir().mkpath(dataDirectory_);
    SessionState state;
    QFile file(sessionPath_);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            state.error = tr("The previous session state could not be read.");
            state.previousRunClean = false;
        } else {
            QJsonParseError parseError{};
            if (file.size() > 4 * 1024 * 1024) {
                state.error = tr("The previous session state is too large to load.");
                state.previousRunClean = false;
            }
            const auto document = state.error.isEmpty()
                ? QJsonDocument::fromJson(file.readAll(), &parseError)
                : QJsonDocument{};
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                state.error = tr("The previous session state is corrupted.");
                state.previousRunClean = false;
            } else {
                const auto root = document.object();
                if (root.value("version").toInt() != 1) {
                    state.error = tr("The previous session uses an unsupported format.");
                    state.previousRunClean = false;
                } else {
                    state.previousRunClean = root.value("cleanShutdown").toBool(false);
                    state.activeIndex = root.value("activeIndex").toInt(-1);
                    const auto entries = root.value("documents").toArray();
                    state.documents.reserve(static_cast<std::size_t>(entries.size()));
                    for (const auto& value : entries) {
                        if (!value.isObject()) continue;
                        const auto item = value.toObject();
                        const auto key = jsonString(item, "recoveryKey");
                        if (!isSafeRecoveryKey(key)) continue;
                        state.documents.push_back({
                            jsonString(item, "path"), key,
                            static_cast<std::uint64_t>(
                                std::max<qint64>(0, item.value("cursorOffset").toInteger()))});
                    }
                    if (state.activeIndex < 0 ||
                        state.activeIndex >= static_cast<int>(state.documents.size()))
                        state.activeIndex = state.documents.empty() ? -1 : 0;
                }
            }
        }
    }
    file.close();
    if (!state.error.isEmpty()) reportError(state.error);
    if (!saveSession(state, false)) {
        reportError(tr("The running session could not be marked as interrupted-safe."));
        if (state.error.isEmpty()) state.error = error_;
    }
    periodicTimer_.start();
    return state;
}

void SessionRecoveryService::setProviders(SessionProvider sessionProvider,
                                          RecoveryProvider recoveryProvider) {
    sessionProvider_ = std::move(sessionProvider);
    recoveryProvider_ = std::move(recoveryProvider);
}

void SessionRecoveryService::clearProviders() {
    periodicTimer_.stop();
    debounceTimer_.stop();
    sessionProvider_ = {};
    recoveryProvider_ = {};
}

void SessionRecoveryService::scheduleSessionSave() {
    if (sessionProvider_) debounceTimer_.start();
}

void SessionRecoveryService::scheduleRecoverySnapshot() {
    if (recoveryProvider_) debounceTimer_.start();
}

void SessionRecoveryService::flushNow() {
    debounceTimer_.stop();
    persistAndSnapshot();
}

void SessionRecoveryService::flushRecoverySnapshotsNow() {
    snapshotModifiedDocuments();
}

void SessionRecoveryService::persistAndSnapshot() {
    if (sessionProvider_ && !saveSession(sessionProvider_(), false))
        reportError(tr("The session state could not be saved."));
    snapshotModifiedDocuments();
}

void SessionRecoveryService::snapshotModifiedDocuments() {
    if (!recoveryProvider_) return;
    std::set<QString> activeKeys;
    for (const auto& document : recoveryProvider_()) {
        if (!document.buffer || document.key.isEmpty()) continue;
        activeKeys.insert(document.key);
        const auto previous = lastSnapshotRevision_.find(document.key);
        if (previous != lastSnapshotRevision_.end() &&
            previous->second == document.revision)
            continue;
        if (saveSnapshot(document))
            lastSnapshotRevision_[document.key] = document.revision;
    }
    for (auto it = lastSnapshotRevision_.begin();
         it != lastSnapshotRevision_.end();) {
        if (activeKeys.contains(it->first)) {
            ++it;
            continue;
        }
        const auto path = snapshotPath(it->first);
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            reportError(tr("An obsolete recovery snapshot could not be removed."));
            ++it;
            continue;
        }
        it = lastSnapshotRevision_.erase(it);
    }
}

bool SessionRecoveryService::saveSession(const SessionState& state, bool clean) const {
    return writeSessionFile(sessionPath_, state, clean);
}

QString SessionRecoveryService::snapshotPath(const QString& key) const {
    if (!isSafeRecoveryKey(key)) return {};
    return QDir(dataDirectory_).filePath(key + QStringLiteral(".recovery"));
}

bool SessionRecoveryService::saveSnapshot(const RecoveryDocument& document) {
    const auto path = snapshotPath(document.key);
    if (path.isEmpty()) {
        reportError(tr("A recovery snapshot has an invalid identity."));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        reportError(tr("A recovery snapshot could not be opened for writing."));
        return false;
    }
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << recoveryMagic << recoveryVersion << document.sourcePath
           << static_cast<quint64>(document.buffer->sizeBytes())
           << static_cast<quint64>(document.buffer->lineCount() - 1);
    if (stream.status() != QDataStream::Ok) {
        reportError(tr("Recovery metadata could not be written."));
        file.cancelWriting();
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    bool lineEndsWritten = true;
    (void)document.buffer->visitSegments(
        [&](std::string_view, std::span<const editor::LineEnding> endings) {
            std::array<char, streamChunkSize> bytes{};
            std::size_t used = 0;
            for (const auto ending : endings) {
                bytes[used++] = static_cast<char>(ending);
                if (used == bytes.size()) {
                    const auto count = static_cast<qint64>(used);
                    if (file.write(bytes.data(), count) != count) {
                        lineEndsWritten = false;
                        return false;
                    }
                    hash.addData(QByteArrayView(bytes.data(), static_cast<qsizetype>(used)));
                    used = 0;
                }
            }
            if (used != 0) {
                const auto count = static_cast<qint64>(used);
                if (file.write(bytes.data(), count) != count) {
                    lineEndsWritten = false;
                    return false;
                }
                hash.addData(QByteArrayView(bytes.data(), static_cast<qsizetype>(used)));
            }
            return true;
        });
    if (!lineEndsWritten) {
        reportError(tr("Recovery line-ending data could not be written."));
        file.cancelWriting();
        return false;
    }
    bool textWritten = true;
    (void)document.buffer->visitSegments(
        [&](std::string_view text, std::span<const editor::LineEnding>) {
            if (file.write(text.data(), static_cast<qint64>(text.size())) !=
                static_cast<qint64>(text.size())) {
                textWritten = false;
                return false;
            }
            hash.addData(QByteArrayView(text.data(),
                                        static_cast<qsizetype>(text.size())));
            return true;
        });
    const auto digest = hash.result();
    if (!textWritten || file.write(digest) != digest.size() || !file.commit()) {
        reportError(tr("The recovery snapshot could not be committed."));
        return false;
    }
    return true;
}

std::vector<RecoveredSnapshot> SessionRecoveryService::recoveries() const {
    std::vector<RecoveredSnapshot> result;
    QDir directory(dataDirectory_);
    for (const auto& name : directory.entryList({"*.recovery"}, QDir::Files)) {
        RecoveredSnapshot snapshot;
        snapshot.key = name.left(name.size() - QStringLiteral(".recovery").size());
        if (!isSafeRecoveryKey(snapshot.key)) {
            snapshot.error = tr("Recovery data has an invalid identity.");
            result.push_back(std::move(snapshot));
            continue;
        }
        QFile file(directory.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            snapshot.error = tr("Recovery data is inaccessible.");
            result.push_back(std::move(snapshot));
            continue;
        }
        QDataStream stream(&file);
        stream.setVersion(QDataStream::Qt_6_0);
        quint32 magic{};
        quint16 version{};
        quint64 textSize{};
        quint64 newlineCount{};
        stream >> magic >> version >> snapshot.sourcePath >> textSize >> newlineCount;
        if (stream.status() != QDataStream::Ok || magic != recoveryMagic ||
            version != recoveryVersion || textSize > std::numeric_limits<std::size_t>::max() ||
            newlineCount > textSize ||
            textSize > static_cast<quint64>(std::numeric_limits<qsizetype>::max()) ||
            newlineCount > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
            snapshot.error = tr("Recovery data has an invalid or unsupported header.");
            result.push_back(std::move(snapshot));
            continue;
        }
        const auto remaining = file.size() - file.pos();
        if (remaining < 32 ||
            textSize > static_cast<quint64>(remaining - 32) ||
            newlineCount != static_cast<quint64>(remaining - 32) - textSize) {
            snapshot.error = tr("Recovery data is truncated or has inconsistent lengths.");
            result.push_back(std::move(snapshot));
            continue;
        }
        editor::TextSnapshot contents;
        contents.lineEndings.resize(static_cast<std::size_t>(newlineCount));
        QCryptographicHash hash(QCryptographicHash::Sha256);
        bool valid = true;
        for (auto& ending : contents.lineEndings) {
            char byte{};
            if (file.getChar(&byte) != true ||
                byte < static_cast<char>(editor::LineEnding::lf) ||
                byte > static_cast<char>(editor::LineEnding::cr)) {
                valid = false;
                break;
            }
            ending = static_cast<editor::LineEnding>(byte);
            hash.addData(QByteArrayView(&byte, 1));
        }
        if (valid) {
            contents.utf8.resize(static_cast<std::size_t>(textSize));
            std::size_t read = 0;
            while (read < contents.utf8.size()) {
                const auto amount = std::min(streamChunkSize,
                                             contents.utf8.size() - read);
                const auto got = file.read(contents.utf8.data() + read,
                                           static_cast<qint64>(amount));
                if (got != static_cast<qint64>(amount)) {
                    valid = false;
                    break;
                }
                hash.addData(QByteArrayView(contents.utf8.data() + read,
                                            static_cast<qsizetype>(amount)));
                read += amount;
            }
        }
        QByteArray expected(32, Qt::Uninitialized);
        if (valid && (file.read(expected.data(), expected.size()) != expected.size() ||
                      !file.atEnd() || hash.result() != expected))
            valid = false;
        if (valid) {
            valid = validUtf8(contents.utf8) &&
                static_cast<std::size_t>(std::count(contents.utf8.begin(),
                                                      contents.utf8.end(), '\n')) ==
                    contents.lineEndings.size();
            if (valid) snapshot.contents = std::move(contents);
        }
        if (!valid)
            snapshot.error = tr("Recovery data is corrupted or incomplete.");
        result.push_back(std::move(snapshot));
    }
    return result;
}

void SessionRecoveryService::removeRecovery(const QString& key) {
    const auto path = snapshotPath(key);
    if (path.isEmpty()) {
        reportError(tr("Recovery data has an invalid identity."));
        return;
    }
    if (QFileInfo::exists(path) && !QFile::remove(path)) {
        reportError(tr("Recovery data could not be removed."));
        return;
    }
    lastSnapshotRevision_.erase(key);
}

void SessionRecoveryService::finishNormally(const SessionState& state,
                                           bool preserveRecoveryForNextStartup) {
    clearProviders();
    if (preserveRecoveryForNextStartup) {
        if (!saveSession(state, false))
            reportError(tr("Recovery data was kept, but the session state could not be saved."));
        return;
    }
    QDir directory(dataDirectory_);
    for (const auto& name : directory.entryList({"*.recovery"}, QDir::Files)) {
        const auto path = directory.filePath(name);
        if (!QFile::remove(path) && QFileInfo::exists(path)) {
            reportError(tr("Recovery data could not be safely cleared."));
            (void)saveSession(state, false);
            return;
        }
    }
    if (!saveSession(state, true)) {
        reportError(tr("The clean session state could not be saved."));
        return;
    }
    lastSnapshotRevision_.clear();
}

QString SessionRecoveryService::errorString() const { return error_; }

void SessionRecoveryService::reportError(const QString& message) {
    if (message.isEmpty() || message == error_) return;
    error_ = message;
    emit persistenceError(error_);
}

} // namespace note::application
