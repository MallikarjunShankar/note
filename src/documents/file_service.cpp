#include "documents/file_service.hpp"

#include <QByteArray>
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

QString toQString(const std::filesystem::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.native());
#else
    const auto utf8 = path.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(utf8.data()), static_cast<qsizetype>(utf8.size()));
#endif
}

std::string utf8(const QString& value) {
    const auto bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

struct DecodedFile {
    QString text;
    FileEncoding encoding{FileEncoding::utf8};
};

bool decodeFile(const QByteArray& bytes, DecodedFile& decoded, std::string& error) {
    qsizetype offset = 0;
    QStringDecoder::Encoding encoding = QStringDecoder::Utf8;
    if (bytes.startsWith(QByteArray::fromHex("fffe0000")) ||
        bytes.startsWith(QByteArray::fromHex("0000feff"))) {
        error = "UTF-32 files are unsupported; convert the file to UTF-8 or UTF-16 first.";
        return false;
    }
    if (bytes.startsWith(QByteArray::fromHex("efbbbf"))) {
        decoded.encoding = FileEncoding::utf8Bom;
        offset = 3;
    } else if (bytes.startsWith(QByteArray::fromHex("fffe"))) {
        decoded.encoding = FileEncoding::utf16LittleEndian;
        encoding = QStringDecoder::Utf16LE;
        offset = 2;
    } else if (bytes.startsWith(QByteArray::fromHex("feff"))) {
        decoded.encoding = FileEncoding::utf16BigEndian;
        encoding = QStringDecoder::Utf16BE;
        offset = 2;
    } else {
        decoded.encoding = FileEncoding::utf8;
    }

    if ((decoded.encoding == FileEncoding::utf16LittleEndian ||
         decoded.encoding == FileEncoding::utf16BigEndian) && ((bytes.size() - offset) % 2) != 0) {
        error = "The file contains malformed UTF-16 data (odd byte count).";
        return false;
    }

    QStringDecoder decoder(encoding);
    decoded.text = decoder(QByteArrayView(bytes.constData() + offset, bytes.size() - offset));
    if (decoder.hasError()) {
        error = decoded.encoding == FileEncoding::utf8
            ? "The file is not valid UTF-8; legacy encodings are not guessed."
            : "The file contains malformed UTF-16 data.";
        return false;
    }
    if (decoded.encoding == FileEncoding::utf16LittleEndian || decoded.encoding == FileEncoding::utf16BigEndian) {
        for (qsizetype i = 0; i < decoded.text.size(); ++i) {
            const auto unit = decoded.text.at(i).unicode();
            if (unit >= 0xd800 && unit <= 0xdbff) {
                if (i + 1 >= decoded.text.size()) {
                    error = "The file contains an unmatched UTF-16 high surrogate.";
                    return false;
                }
                const auto next = decoded.text.at(i + 1).unicode();
                if (next < 0xdc00 || next > 0xdfff) {
                    error = "The file contains an unmatched UTF-16 high surrogate.";
                    return false;
                }
                ++i;
            } else if (unit >= 0xdc00 && unit <= 0xdfff) {
                error = "The file contains an unmatched UTF-16 low surrogate.";
                return false;
            }
        }
    }
    return true;
}

editor::TextSnapshot normalizeLineEndings(const QString& text) {
    QString canonical;
    canonical.reserve(text.size());
    editor::TextSnapshot snapshot;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const auto character = text.at(i);
        if (character == QLatin1Char('\r')) {
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n')) {
                snapshot.lineEndings.push_back(editor::LineEnding::crlf);
                ++i;
            } else {
                snapshot.lineEndings.push_back(editor::LineEnding::cr);
            }
            canonical.append(QLatin1Char('\n'));
        } else if (character == QLatin1Char('\n')) {
            snapshot.lineEndings.push_back(editor::LineEnding::lf);
            canonical.append(QLatin1Char('\n'));
        } else {
            canonical.append(character);
        }
    }
    const auto bytes = canonical.toUtf8();
    snapshot.utf8.assign(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    return snapshot;
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

bool writeAll(QSaveFile& file, const char* data, qsizetype size) {
    qsizetype written = 0;
    while (written < size) {
        const auto count = file.write(data + written, size - written);
        if (count <= 0) return false;
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

FileOperationResult writeDocument(Document& document, const std::filesystem::path& path,
                                  std::uint64_t& savedRevision, DocumentMetadata& savedMetadata) {
    savedRevision = document.buffer().revision();
    savedMetadata = document.metadata();
    const auto snapshot = document.buffer().snapshot();
    const auto qpath = toQString(path);
    QSaveFile file(qpath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return {false, utf8(file.errorString())};

    if (document.metadata().encoding == FileEncoding::utf8Bom && !writeAll(file, "\xef\xbb\xbf", 3)) {
        const auto message = utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }
    if (document.metadata().encoding == FileEncoding::utf16LittleEndian && !writeAll(file, "\xff\xfe", 2)) {
        const auto message = utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }
    if (document.metadata().encoding == FileEncoding::utf16BigEndian && !writeAll(file, "\xfe\xff", 2)) {
        const auto message = utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }

    constexpr std::size_t chunkLimit = 64 * 1024;
    std::string chunk;
    chunk.reserve(chunkLimit + 4);
    std::size_t newlineIndex = 0;
    auto flush = [&]() -> bool {
        if (chunk.empty()) return true;
        bool ok = false;
        if (document.metadata().encoding == FileEncoding::utf16LittleEndian ||
            document.metadata().encoding == FileEncoding::utf16BigEndian) {
            const auto encoded = encodeUtf16(chunk, document.metadata().encoding);
            ok = writeAll(file, encoded.constData(), encoded.size());
        } else {
            ok = writeAll(file, chunk.data(), static_cast<qsizetype>(chunk.size()));
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

    std::size_t cursor = 0;
    while (cursor < snapshot.utf8.size()) {
        const auto newline = snapshot.utf8.find('\n', cursor);
        const auto end = newline == std::string::npos ? snapshot.utf8.size() : newline;
        if (!appendBytes(std::string_view(snapshot.utf8).substr(cursor, end - cursor))) {
            const auto message = utf8(file.errorString());
            file.cancelWriting();
            return {false, message};
        }
        if (newline == std::string::npos) break;
        std::string_view ending;
        switch (snapshot.lineEndings.at(newlineIndex++)) {
        case editor::LineEnding::lf: ending = "\n"; break;
        case editor::LineEnding::crlf: ending = "\r\n"; break;
        case editor::LineEnding::cr: ending = "\r"; break;
        }
        if (!appendBytes(ending)) {
            const auto message = utf8(file.errorString());
            file.cancelWriting();
            return {false, message};
        }
        cursor = newline + 1;
    }
    if (!flush()) {
        const auto message = utf8(file.errorString());
        file.cancelWriting();
        return {false, message};
    }
    if (newlineIndex != snapshot.lineEndings.size()) {
        file.cancelWriting();
        return {false, "Text buffer newline metadata is inconsistent."};
    }
    if (!file.commit()) return {false, utf8(file.errorString())};

    const QFileInfo info(qpath);
    savedMetadata.path = path;
    savedMetadata.file = fileMetadata(info);
    if (!snapshot.lineEndings.empty()) {
        savedMetadata.preferredLineEnding = preferredEnding(snapshot.lineEndings);
        savedMetadata.mixedLineEndings = mixedEndings(snapshot.lineEndings);
    }
    return {true, {}};
}

} // namespace

OpenFileResult FileService::open(const std::filesystem::path& path) const {
    const auto qpath = toQString(path);
    QFile file(qpath);
    if (!file.open(QIODevice::ReadOnly)) return {nullptr, {}, utf8(file.errorString())};
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return {nullptr, {}, utf8(file.errorString())};

    DecodedFile decoded;
    std::string error;
    if (!decodeFile(bytes, decoded, error)) return {nullptr, {}, std::move(error)};
    auto snapshot = normalizeLineEndings(decoded.text);
    DocumentMetadata metadata;
    metadata.path = path;
    metadata.encoding = decoded.encoding;
    metadata.preferredLineEnding = preferredEnding(snapshot.lineEndings);
    metadata.mixedLineEndings = mixedEndings(snapshot.lineEndings);
    metadata.file = fileMetadata(QFileInfo(qpath));
    auto buffer = std::make_unique<editor::TextBuffer>(std::move(snapshot));
    return {std::move(buffer), std::move(metadata), {}};
}

FileOperationResult FileService::save(Document& document) const {
    if (!document.metadata().path) return {false, "Document has no associated path; use Save As."};
    std::uint64_t revision = 0;
    DocumentMetadata metadata;
    auto result = writeDocument(document, *document.metadata().path, revision, metadata);
    if (result) (void)document.recordSuccessfulSave(revision, std::move(metadata));
    return result;
}

FileOperationResult FileService::saveAs(Document& document, const std::filesystem::path& path) const {
    if (path.empty()) return {false, "Save path is empty."};
    std::uint64_t revision = 0;
    DocumentMetadata metadata;
    auto result = writeDocument(document, path, revision, metadata);
    if (result) (void)document.recordSuccessfulSave(revision, std::move(metadata));
    return result;
}

} // namespace note::documents
