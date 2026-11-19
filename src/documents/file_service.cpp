#include "documents/file_service.hpp"
#include "platform/path_resolver.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QString>
#include <QStringDecoder>
#include <QStringEncoder>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace note::documents {
namespace {

std::string utf8(const QString& value) {
    const auto bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

editor::LineEnding preferredEnding(const std::vector<editor::LineEnding>& endings) {
    std::array<std::size_t, 3> counts{};
    for (const auto ending : endings) ++counts[static_cast<std::size_t>(ending)];
    const auto best = static_cast<std::size_t>(std::distance(counts.begin(), std::max_element(counts.begin(), counts.end())));
    return static_cast<editor::LineEnding>(best);
}

bool mixedEndings(const std::vector<editor::LineEnding>& endings) {
    if (endings.empty()) return false;
    return std::any_of(endings.begin() + 1, endings.end(), [&](auto ending) { return ending != endings.front(); });
}

bool writeAll(QSaveFile& file, const char* data, qsizetype size, QCryptographicHash* hash = nullptr) {
    qsizetype written = 0;
    while (written < size) {
        const auto count = file.write(data + written, size - written);
        if (count <= 0) return false;
        if (hash) hash->addData(QByteArrayView(data + written, count));
        written += count;
    }
    return true;
}

QByteArray encodeUtf16(std::string_view input, FileEncoding encoding) {
    const auto text = QString::fromUtf8(input.data(), static_cast<qsizetype>(input.size()));
    QStringEncoder encoder(encoding == FileEncoding::utf16LittleEndian
                               ? QStringEncoder::Utf16LE : QStringEncoder::Utf16BE);
    return encoder(text);
}

FileMetadata fileMetadata(const QFileInfo& info) {
    return {static_cast<std::uintmax_t>(info.size()), info.lastModified().toMSecsSinceEpoch()};
}

FileInspection inspectFile(const std::filesystem::path& path) {
    const auto qpath = platform::pathToQString(path);
    QFile file(qpath);
    if (!file.exists()) return {false, {}, {}};
    if (!file.open(QIODevice::ReadOnly)) {
        if (!QFileInfo::exists(qpath)) return {false, {}, {}};
        return {true, {}, utf8(file.errorString())};
    }
    const QFileInfo before(file);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto bytes = file.read(1024 * 1024);
        if (bytes.isEmpty() && file.error() != QFileDevice::NoError)
            return {true, {}, utf8(file.errorString())};
        if (bytes.isEmpty()) break;
        hash.addData(bytes);
    }
    if (file.error() != QFileDevice::NoError) return {true, {}, utf8(file.errorString())};
    const QFileInfo after(qpath);
    if (!after.exists()) return {false, {}, {}};
    if (before.size() != after.size() ||
        before.lastModified().toMSecsSinceEpoch() != after.lastModified().toMSecsSinceEpoch())
        return {true, {}, "The file changed while its contents were being checked."};
    auto metadata = fileMetadata(after);
    const auto digest = hash.result();
    std::copy_n(reinterpret_cast<const std::uint8_t*>(digest.constData()), metadata.sha256.size(), metadata.sha256.begin());
    metadata.hasFingerprint = true;
    return {true, metadata, {}};
}

FileOperationResult writeDocument(Document& document, const std::filesystem::path& path,
                                  std::uint64_t& savedRevision, DocumentMetadata& savedMetadata,
                                  std::optional<std::array<std::uint8_t, 32>> expectedFingerprint,
                                  bool expectAbsent) {
    savedRevision = document.buffer().revision();
    savedMetadata = document.metadata();
    const auto qpath = platform::pathToQString(path);
    QCryptographicHash contentHash(QCryptographicHash::Sha256);
    QSaveFile file(qpath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return {false, utf8(file.errorString())};

    if (document.metadata().encoding == FileEncoding::utf8Bom && !writeAll(file, "\xef\xbb\xbf", 3, &contentHash)) {
        const auto message = utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }
    if (document.metadata().encoding == FileEncoding::utf16LittleEndian && !writeAll(file, "\xff\xfe", 2, &contentHash)) {
        const auto message = utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }
    if (document.metadata().encoding == FileEncoding::utf16BigEndian && !writeAll(file, "\xfe\xff", 2, &contentHash)) {
        const auto message = utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }

    constexpr std::size_t chunkLimit = 64 * 1024;
    std::string chunk;
    chunk.reserve(chunkLimit + 4);
    std::size_t newlineCount = 0;
    auto flush = [&]() -> bool {
        if (chunk.empty()) return true;
        bool ok = false;
        if (document.metadata().encoding == FileEncoding::utf16LittleEndian ||
            document.metadata().encoding == FileEncoding::utf16BigEndian) {
            const auto encoded = encodeUtf16(chunk, document.metadata().encoding);
            ok = writeAll(file, encoded.constData(), encoded.size(), &contentHash);
        } else {
            ok = writeAll(file, chunk.data(), static_cast<qsizetype>(chunk.size()), &contentHash);
        }
        chunk.clear();
        return ok;
    };
    auto appendBytes = [&](std::string_view bytes) -> bool {
        while (!bytes.empty()) {
            const auto available = chunkLimit - chunk.size();
            if (available == 0 && !flush()) return false;
            const auto amount = std::min(available == 0 ? chunkLimit : available, bytes.size());
            chunk.append(bytes.data(), amount);
            bytes.remove_prefix(amount);
            if (chunk.size() >= chunkLimit) {
                std::size_t safe = chunk.size();
                if (document.metadata().encoding == FileEncoding::utf16LittleEndian ||
                    document.metadata().encoding == FileEncoding::utf16BigEndian) {
                    if (!bytes.empty() && (static_cast<unsigned char>(bytes.front()) & 0xc0) == 0x80) {
                        safe = chunk.size() - 1;
                        while (safe > 0 &&
                               (static_cast<unsigned char>(chunk[safe]) & 0xc0) == 0x80) --safe;
                    }
                }
                if (safe > 0 && safe < chunk.size()) {
                    auto remainder = chunk.substr(safe);
                    chunk.resize(safe);
                    if (!flush()) return false;
                    chunk = std::move(remainder);
                } else if (safe == chunk.size() && !flush()) {
                    return false;
                }
            }
        }
        return true;
    };

    std::array<std::size_t, 3> endingCounts{};
    editor::LineEnding firstEnding = editor::LineEnding::lf;
    bool hasFirstEnding = false;
    bool hasMixedEndings = false;
    bool inconsistentEndings = false;
    const auto completed = document.buffer().visitSegments(
        [&](std::string_view text, std::span<const editor::LineEnding> endings) {
            std::size_t cursor = 0;
            std::size_t endingIndex = 0;
            while (cursor < text.size()) {
                const auto newline = text.find('\n', cursor);
                const auto end = newline == std::string_view::npos
                    ? text.size() : newline;
                if (!appendBytes(text.substr(cursor, end - cursor))) return false;
                if (newline == std::string_view::npos) break;
                if (endingIndex >= endings.size()) {
                    inconsistentEndings = true;
                    return false;
                }
                const auto ending = endings[endingIndex++];
                std::string_view endingBytes;
                switch (ending) {
                case editor::LineEnding::lf: endingBytes = "\n"; break;
                case editor::LineEnding::crlf: endingBytes = "\r\n"; break;
                case editor::LineEnding::cr: endingBytes = "\r"; break;
                default:
                    inconsistentEndings = true;
                    return false;
                }
                ++endingCounts[static_cast<std::size_t>(ending)];
                ++newlineCount;
                if (!hasFirstEnding) {
                    firstEnding = ending;
                    hasFirstEnding = true;
                } else if (firstEnding != ending) {
                    hasMixedEndings = true;
                }
                if (!appendBytes(endingBytes)) return false;
                cursor = newline + 1;
            }
            if (endingIndex != endings.size()) {
                inconsistentEndings = true;
                return false;
            }
            return true;
        });
    if (!completed) {
        const auto message = inconsistentEndings
            ? std::string("Text buffer newline metadata is inconsistent.")
            : utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }
    if (newlineCount != document.buffer().lineCount() - 1) {
        file.cancelWriting();
        return {false, "Text buffer newline metadata is inconsistent."};
    }
    if (!flush()) {
        const auto message = utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }
    const auto current = inspectFile(path);
    if (!current.error.empty()) {
        file.cancelWriting();
        return {false, current.error};
    }
    if ((expectAbsent && current.exists) ||
        (expectedFingerprint && (!current.exists || current.metadata.sha256 != *expectedFingerprint))) {
        file.cancelWriting();
        return {false, "The destination changed on disk while the save was being prepared. Your document is unchanged; use Save As or reload.", true, !current.exists};
    }
    if (!file.commit()) return {false, utf8(file.errorString())};

    const QFileInfo info(qpath);
    savedMetadata.path = path;
    savedMetadata.file = fileMetadata(info);
    const auto digest = contentHash.result();
    std::copy_n(reinterpret_cast<const std::uint8_t*>(digest.constData()), savedMetadata.file->sha256.size(),
                savedMetadata.file->sha256.begin());
    savedMetadata.file->hasFingerprint = true;
    if (hasFirstEnding) {
        const auto best = static_cast<std::size_t>(
            std::distance(endingCounts.begin(),
                          std::max_element(endingCounts.begin(), endingCounts.end())));
        savedMetadata.preferredLineEnding = static_cast<editor::LineEnding>(best);
        savedMetadata.mixedLineEndings = hasMixedEndings;
    }
    return {true, {}};
}

} // namespace

OpenFileResult FileService::open(const std::filesystem::path& path) const {
    return open(path, OpenOptions{});
}

OpenFileResult FileService::open(const std::filesystem::path& path, const OpenOptions& options) const {
    if (options.cancelled && options.cancelled->load()) return {nullptr, {}, "Opening was cancelled."};
    const auto qpath = platform::pathToQString(path);
    QFile file(qpath);
    if (!file.open(QIODevice::ReadOnly)) return {nullptr, {}, utf8(file.errorString())};
    const QFileInfo initialInfo(file);
    const auto expectedSize = static_cast<std::uintmax_t>(std::max<qint64>(0, initialInfo.size()));
    const auto initialModified = initialInfo.lastModified().toMSecsSinceEpoch();
    const auto chunkSize = static_cast<qsizetype>(std::clamp<std::size_t>(options.chunkBytes, 1, 16 * 1024 * 1024));
    QByteArray first = file.read(std::max<qsizetype>(chunkSize, 4));
    if (file.error() != QFileDevice::NoError) return {nullptr, {}, utf8(file.errorString())};
    if (options.cancelled && options.cancelled->load()) return {nullptr, {}, "Opening was cancelled."};
    QCryptographicHash inputHash(QCryptographicHash::Sha256);
    inputHash.addData(first);

    FileEncoding fileEncoding = FileEncoding::utf8;
    QStringDecoder::Encoding encoding = QStringDecoder::Utf8;
    qsizetype offset = 0;
    const QByteArray utf32le = QByteArray::fromHex("fffe0000");
    const QByteArray utf32be = QByteArray::fromHex("0000feff");
    if (first.startsWith(utf32le) || first.startsWith(utf32be))
        return {nullptr, {}, "UTF-32 files are unsupported; convert the file to UTF-8 or UTF-16 first."};
    if (first.startsWith(QByteArray::fromHex("efbbbf"))) { fileEncoding = FileEncoding::utf8Bom; offset = 3; }
    else if (first.startsWith(QByteArray::fromHex("fffe"))) { fileEncoding = FileEncoding::utf16LittleEndian; encoding = QStringDecoder::Utf16LE; offset = 2; }
    else if (first.startsWith(QByteArray::fromHex("feff"))) { fileEncoding = FileEncoding::utf16BigEndian; encoding = QStringDecoder::Utf16BE; offset = 2; }

    QStringDecoder decoder(encoding);
    bool pendingHighSurrogate = false;
    bool invalidUtf16 = false;
    QChar pendingHighUnit;
    int utf8Continuation = 0;
    unsigned char utf8NextMin = 0x80;
    unsigned char utf8NextMax = 0xbf;
    bool invalidUtf8 = false;
    auto validateUtf8 = [&](QByteArrayView bytes) {
        if (fileEncoding != FileEncoding::utf8 && fileEncoding != FileEncoding::utf8Bom) return true;
        for (const auto raw : bytes) {
            const auto byte = static_cast<unsigned char>(raw);
            if (utf8Continuation != 0) {
                if (byte < utf8NextMin || byte > utf8NextMax) return false;
                --utf8Continuation;
                utf8NextMin = 0x80; utf8NextMax = 0xbf;
            } else if (byte <= 0x7f) continue;
            else if (byte >= 0xc2 && byte <= 0xdf) utf8Continuation = 1;
            else if (byte == 0xe0) { utf8Continuation = 2; utf8NextMin = 0xa0; }
            else if ((byte >= 0xe1 && byte <= 0xec) || (byte >= 0xee && byte <= 0xef)) utf8Continuation = 2;
            else if (byte == 0xed) { utf8Continuation = 2; utf8NextMax = 0x9f; }
            else if (byte == 0xf0) { utf8Continuation = 3; utf8NextMin = 0x90; }
            else if (byte >= 0xf1 && byte <= 0xf3) utf8Continuation = 3;
            else if (byte == 0xf4) { utf8Continuation = 3; utf8NextMax = 0x8f; }
            else return false;
        }
        return true;
    };
    editor::TextSnapshot snapshot;
    snapshot.utf8.reserve(static_cast<std::size_t>(expectedSize));
    snapshot.lineEndings.reserve(static_cast<std::size_t>(expectedSize / 32));
    bool pendingCr = false;
    auto appendDecoded = [&](const QString& text) {
        if (text.isEmpty()) return;
        QString canonical;
        canonical.reserve(text.size() + (pendingCr ? 1 : 0));
        qsizetype i = 0;
        if (pendingCr) {
            if (!text.isEmpty() && text.front() == QLatin1Char('\n')) {
                canonical.append(QLatin1Char('\n')); snapshot.lineEndings.push_back(editor::LineEnding::crlf); i = 1;
            } else {
                canonical.append(QLatin1Char('\n')); snapshot.lineEndings.push_back(editor::LineEnding::cr);
            }
            pendingCr = false;
        }
        for (; i < text.size(); ++i) {
            const auto ch = text.at(i);
            if (ch == QLatin1Char('\r')) {
                if (i + 1 < text.size()) {
                    if (text.at(i + 1) == QLatin1Char('\n')) { canonical.append(QLatin1Char('\n')); snapshot.lineEndings.push_back(editor::LineEnding::crlf); ++i; }
                    else { canonical.append(QLatin1Char('\n')); snapshot.lineEndings.push_back(editor::LineEnding::cr); }
                } else pendingCr = true;
            } else if (ch == QLatin1Char('\n')) {
                canonical.append(ch); snapshot.lineEndings.push_back(editor::LineEnding::lf);
            } else canonical.append(ch);
        }
        const auto bytes = canonical.toUtf8();
        snapshot.utf8.append(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    };
    std::uintmax_t bytesRead = 0;
    auto decodeChunk = [&](QByteArrayView bytes) -> bool {
        if (!validateUtf8(bytes)) { invalidUtf8 = true; return false; }
        const QString text = decoder(bytes);
        if (decoder.hasError()) return false;
        if (fileEncoding != FileEncoding::utf16LittleEndian && fileEncoding != FileEncoding::utf16BigEndian) {
            appendDecoded(text);
            return true;
        }
        QString ready;
        qsizetype start = 0;
        if (pendingHighSurrogate) {
            if (text.isEmpty()) return true;
            const auto first = text.front().unicode();
            if (first < 0xdc00 || first > 0xdfff) { invalidUtf16 = true; return false; }
            ready.append(pendingHighUnit);
            ready.append(text.front());
            start = 1;
            pendingHighSurrogate = false;
        }
        auto end = text.size();
        if (end > start && text.at(end - 1).unicode() >= 0xd800 && text.at(end - 1).unicode() <= 0xdbff) {
            pendingHighUnit = text.at(end - 1);
            pendingHighSurrogate = true;
            --end;
        }
        for (auto i = start; i < end; ++i) {
            const auto unit = text.at(i).unicode();
            if (unit >= 0xd800 && unit <= 0xdbff) {
                if (i + 1 >= end) { invalidUtf16 = true; return false; }
                const auto next = text.at(i + 1).unicode();
                if (next < 0xdc00 || next > 0xdfff) { invalidUtf16 = true; return false; }
                ready.append(text.at(i));
                ready.append(text.at(++i));
            } else if (unit >= 0xdc00 && unit <= 0xdfff) { invalidUtf16 = true; return false; }
            else ready.append(text.at(i));
        }
        appendDecoded(ready);
        return true;
    };
    if (!decodeChunk(QByteArrayView(first.constData() + offset, first.size() - offset))) {
        const bool utf16 = fileEncoding == FileEncoding::utf16LittleEndian || fileEncoding == FileEncoding::utf16BigEndian;
        return {nullptr, {}, utf16 ? "The file contains malformed UTF-16 data." : "The file is not valid UTF-8; legacy encodings are not guessed."};
    }
    bytesRead = static_cast<std::uintmax_t>(first.size());
    if (options.progress) options.progress({bytesRead, expectedSize, false});
    while (!file.atEnd()) {
        if (options.cancelled && options.cancelled->load()) return {nullptr, {}, "Opening was cancelled."};
        const auto bytes = file.read(chunkSize);
        if (bytes.isEmpty() && file.error() != QFileDevice::NoError) return {nullptr, {}, utf8(file.errorString())};
        if (bytes.isEmpty()) break;
        inputHash.addData(bytes);
        if (!decodeChunk(QByteArrayView(bytes.constData(), bytes.size()))) {
            const bool utf16 = fileEncoding == FileEncoding::utf16LittleEndian || fileEncoding == FileEncoding::utf16BigEndian;
            return {nullptr, {}, utf16 ? "The file contains malformed UTF-16 data." : "The file is not valid UTF-8; legacy encodings are not guessed."};
        }
        bytesRead += static_cast<std::uintmax_t>(bytes.size());
        if (options.progress) options.progress({bytesRead, expectedSize, false});
    }
    if (file.error() != QFileDevice::NoError) return {nullptr, {}, utf8(file.errorString())};
    if (invalidUtf8 || invalidUtf16 || pendingHighSurrogate ||
        ((fileEncoding == FileEncoding::utf8 || fileEncoding == FileEncoding::utf8Bom) && utf8Continuation != 0) ||
        decoder.hasError() || ((fileEncoding == FileEncoding::utf16LittleEndian ||
         fileEncoding == FileEncoding::utf16BigEndian) && ((bytesRead - static_cast<std::uintmax_t>(offset)) % 2) != 0)) {
        const bool utf16 = fileEncoding == FileEncoding::utf16LittleEndian || fileEncoding == FileEncoding::utf16BigEndian;
        return {nullptr, {}, utf16 ? "The file contains malformed UTF-16 data." : "The file is not valid UTF-8; legacy encodings are not guessed."};
    }
    if (pendingCr) { snapshot.utf8.push_back('\n'); snapshot.lineEndings.push_back(editor::LineEnding::cr); }
    if (options.cancelled && options.cancelled->load()) return {nullptr, {}, "Opening was cancelled."};
    const QFileInfo finalInfo(qpath);
    if (!finalInfo.exists() || static_cast<std::uintmax_t>(std::max<qint64>(0, finalInfo.size())) != expectedSize ||
        finalInfo.lastModified().toMSecsSinceEpoch() != initialModified)
        return {nullptr, {}, "The file changed while it was being opened. Please try again."};
    if (options.progress) options.progress({bytesRead, expectedSize, true});
    DocumentMetadata metadata;
    metadata.path = path;
    metadata.encoding = fileEncoding;
    metadata.preferredLineEnding = preferredEnding(snapshot.lineEndings);
    metadata.mixedLineEndings = mixedEndings(snapshot.lineEndings);
    metadata.file = fileMetadata(finalInfo);
    const auto digest = inputHash.result();
    std::copy_n(reinterpret_cast<const std::uint8_t*>(digest.constData()), metadata.file->sha256.size(),
                metadata.file->sha256.begin());
    metadata.file->hasFingerprint = true;
    std::unique_ptr<editor::TextBuffer> buffer;
    try { buffer = std::make_unique<editor::TextBuffer>(std::move(snapshot)); }
    catch (const std::exception&) {
        return {nullptr, {}, fileEncoding == FileEncoding::utf8 || fileEncoding == FileEncoding::utf8Bom
            ? "The file is not valid UTF-8; legacy encodings are not guessed."
            : "The file contains malformed UTF-16 data."};
    }
    if (options.cancelled && options.cancelled->load()) return {nullptr, {}, "Opening was cancelled."};
    const auto finalInspection = inspectFile(path);
    if (!finalInspection.exists || !finalInspection.error.empty() ||
        finalInspection.metadata.sha256 != metadata.file->sha256)
        return {nullptr, {}, "The file changed while it was being opened. Please try again."};
    return {std::move(buffer), std::move(metadata), {}};
}

FileInspection FileService::inspect(const std::filesystem::path& path) const {
    return inspectFile(path);
}

FileOperationResult FileService::save(Document& document) const {
    if (!document.metadata().path) return {false, "Document has no associated path; use Save As."};
    if (!document.metadata().file || !document.metadata().file->hasFingerprint)
        return {false, "Cannot verify the saved file contents. Use Save As to write to a new path.", true};
    const auto current = inspect(*document.metadata().path);
    if (!current.error.empty()) return {false, current.error};
    if (!current.exists || current.metadata.sha256 != document.metadata().file->sha256)
        return {false, current.exists
            ? "The file changed outside Note. Your edits are safe; use Save As or reload the disk copy."
            : "The file was removed outside Note. Your edits are safe; use Save As to keep them.",
            true, !current.exists};
    std::uint64_t revision = 0;
    DocumentMetadata metadata;
    auto result = writeDocument(document, *document.metadata().path, revision, metadata,
                                current.metadata.sha256, false);
    if (result) (void)document.recordSuccessfulSave(revision, std::move(metadata));
    return result;
}

FileOperationResult FileService::saveAs(Document& document, const std::filesystem::path& path,
                                        bool allowOverwrite,
                                        std::optional<std::array<std::uint8_t, 32>> expectedTargetFingerprint) const {
    if (path.empty()) return {false, "Save path is empty."};
    const auto targetState = inspect(path);
    if (!targetState.error.empty()) return {false, targetState.error};
    if (document.metadata().path) {
        std::error_code ec1, ec2;
        const auto source = std::filesystem::weakly_canonical(*document.metadata().path, ec1);
        const auto target = std::filesystem::weakly_canonical(path, ec2);
        if (!ec1 && !ec2 && source == target && targetState.exists) return save(document);
    }
    if (targetState.exists && !allowOverwrite)
        return {false, "The destination already exists. Confirm replacement or choose another path.", true};
    if (targetState.exists && (!expectedTargetFingerprint ||
        targetState.metadata.sha256 != *expectedTargetFingerprint))
        return {false, "The destination changed since it was confirmed. Choose it again before replacing it.", true};
    std::uint64_t revision = 0;
    DocumentMetadata metadata;
    auto result = writeDocument(document, path, revision, metadata,
                                targetState.exists ? expectedTargetFingerprint : std::nullopt,
                                !targetState.exists);
    if (result) (void)document.recordSuccessfulSave(revision, std::move(metadata));
    return result;
}

} // namespace note::documents
