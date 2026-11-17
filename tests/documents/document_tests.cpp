#include "documents/document_manager.hpp"
#include "documents/file_service.hpp"

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using note::editor::LineEnding;
using note::editor::TextBuffer;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string normalized(std::string_view input) {
    std::string out;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r') {
            if (i + 1 < input.size() && input[i + 1] == '\n') ++i;
            out.push_back('\n');
        } else out.push_back(input[i]);
    }
    return out;
}

std::vector<std::size_t> boundaries(const std::string& text) {
    std::vector<std::size_t> result{0};
    for (std::size_t i = 1; i <= text.size(); ++i) {
        if (i == text.size() || (static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) result.push_back(i);
    }
    return result;
}

QString qpath(const std::filesystem::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    const auto value = path.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(value.data()), static_cast<qsizetype>(value.size()));
#endif
}

std::filesystem::path pathFromQString(const QString& value) {
#if defined(_WIN32)
    return std::filesystem::path(value.toStdWString());
#else
    const auto bytes = value.toUtf8();
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(bytes.constData()),
                                               static_cast<std::size_t>(bytes.size())));
#endif
}

QByteArray readBytes(const std::filesystem::path& path) {
    QFile file(qpath(path));
    require(file.open(QIODevice::ReadOnly), "test file open failed");
    return file.readAll();
}

void writeBytes(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(qpath(path));
    require(file.open(QIODevice::WriteOnly), "test file create failed");
    require(file.write(bytes) == bytes.size(), "test file write failed");
}

void testTextBuffer() {
    TextBuffer empty;
    require(empty.sizeBytes() == 0 && empty.lineCount() == 1, "empty buffer shape");
    require(empty.lineText(0).empty(), "empty buffer line");
    TextBuffer lines("first\r\n\rsecond\n");
    require(lines.text() == "first\n\nsecond\n", "newline normalization");
    require(lines.lineCount() == 4, "trailing empty line count");
    require(lines.lineText(0) == "first" && lines.lineText(1).empty() &&
            lines.lineText(2) == "second" && lines.lineText(3).empty(), "line retrieval");
    const auto lineSnapshot = lines.snapshot();
    require(lineSnapshot.lineEndings == std::vector<LineEnding>{LineEnding::crlf, LineEnding::cr,
            LineEnding::lf}, "line-ending metadata");

    TextBuffer defaultEnding("a\r\nb");
    defaultEnding.insert(defaultEnding.sizeBytes(), "\nnext", LineEnding::crlf);
    require(defaultEnding.snapshot().lineEndings == std::vector<LineEnding>{LineEnding::crlf,
            LineEnding::crlf}, "preferred newline style for inserted lines");
    const auto stableRevision = defaultEnding.revision();
    const auto stablePieces = defaultEnding.pieceCount();
    defaultEnding.erase(0, 0);
    defaultEnding.insert(0, "");
    require(defaultEnding.revision() == stableRevision && defaultEnding.pieceCount() == stablePieces,
            "empty edit is a no-op");

    TextBuffer crlf("a\r\nb");
    crlf.insert(crlf.sizeBytes(), "\nnext", LineEnding::crlf);
    require(crlf.snapshot().lineEndings == std::vector<LineEnding>{LineEnding::crlf, LineEnding::crlf},
            "preferred newline style for inserted lines");
    const auto originalRevision = crlf.revision();
    const auto originalPieces = crlf.pieceCount();
    crlf.erase(0, 0);
    crlf.insert(0, "");
    require(crlf.revision() == originalRevision && crlf.pieceCount() == originalPieces, "empty edit is a no-op");

    lines.insert(0, "start\n");
    lines.insert(lines.sizeBytes(), "end");
    require(lines.lineText(0) == "start", "beginning insertion");
    require(lines.lineText(lines.lineCount() - 1) == "end", "end insertion");
    lines.replace(0, 5, "head");
    require(lines.lineText(0) == "head", "replacement");
    lines.erase(0, 4);
    require(lines.lineText(0).empty(), "beginning deletion");

    TextBuffer unicode("aλ🙂z");
    require(unicode.sizeBytes() == 8, "UTF-8 byte offsets");
    require(unicode.textRange(1, 2) == "λ", "UTF-8 range retrieval");
    bool splitCodepointRejected = false;
    try { unicode.erase(2, 1); } catch (const std::invalid_argument&) { splitCodepointRejected = true; }
    require(splitCodepointRejected, "reject split UTF-8 code point");
    unicode.insert(3, "é");
    require(unicode.text() == "aλé🙂z", "Unicode insertion");

    TextBuffer sequential("base");
    sequential.insert(4, "a");
    sequential.insert(5, "b");
    require(sequential.text() == "baseab" && sequential.pieceCount() == 2,
            "adjacent sequential typing coalesces pieces");
    TextBuffer edits("abcdef");
    edits.insert(3, "XYZ");
    edits.erase(1, 2);
    edits.erase(edits.sizeBytes() - 2, 2);
    edits.replace(0, 1, "Q");
    require(edits.text() == "QXYZd", "middle/end deletion and replacement");

    std::string longLine(300000, 'x');
    TextBuffer longBuffer(longLine);
    require(longBuffer.lineCount() == 1 && longBuffer.lineText(0).size() == longLine.size(), "very long line");

    std::mt19937 random(0x4e4f5445);
    std::string reference = "alpha\nbeta\n";
    TextBuffer fragmented(reference);
    const std::vector<std::string> inserts{"x", "λ", "🙂", "\n", "\r\n", "r\ns", ""};
    for (int operation = 0; operation < 4000; ++operation) {
        const auto points = boundaries(reference);
        const auto first = points[random() % points.size()];
        if ((random() & 3U) == 0 && first < reference.size()) {
            const auto remaining = std::vector<std::size_t>(points.begin() +
                static_cast<std::ptrdiff_t>(std::lower_bound(points.begin(), points.end(), first) - points.begin()), points.end());
            const auto last = remaining[random() % remaining.size()];
            fragmented.erase(first, last - first);
            reference.erase(first, last - first);
        } else {
            const auto& addition = inserts[random() % inserts.size()];
            fragmented.insert(first, addition);
            reference.insert(first, normalized(addition));
        }
        require(fragmented.text() == reference, "randomized differential text mismatch");
        const auto updatedPoints = boundaries(reference);
        const auto rangeStart = updatedPoints[random() % updatedPoints.size()];
        const auto rangeEndOptions = std::vector<std::size_t>(
            updatedPoints.begin() + static_cast<std::ptrdiff_t>(std::lower_bound(updatedPoints.begin(), updatedPoints.end(), rangeStart) - updatedPoints.begin()),
            updatedPoints.end());
        const auto rangeEnd = rangeEndOptions[random() % rangeEndOptions.size()];
        require(fragmented.textRange(rangeStart, rangeEnd - rangeStart) ==
                reference.substr(rangeStart, rangeEnd - rangeStart), "randomized range retrieval mismatch");
        const auto expectedLines = static_cast<std::size_t>(std::count(reference.begin(), reference.end(), '\n')) + 1;
        require(fragmented.lineCount() == expectedLines, "randomized line count mismatch");
        std::size_t start = 0;
        for (std::size_t line = 0; line < expectedLines; ++line) {
            const auto end = reference.find('\n', start);
            const auto expectedEnd = end == std::string::npos ? reference.size() : end;
            const auto range = fragmented.lineRange(line);
            require(range.begin == start && range.end == expectedEnd, "randomized line index mismatch");
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    require(fragmented.pieceCount() > 1, "fragmentation workload did not fragment");
}

void testDocumentsAndFiles() {
    using namespace note::documents;
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    const auto root = pathFromQString(temporary.path());
    const auto source = root / "mixed.txt";
    const QByteArray mixed("one\r\ntwo\rthree\nfour", 19);
    writeBytes(source, mixed);

    FileService service;
    auto opened = service.open(source);
    require(static_cast<bool>(opened), "open mixed line endings");
    require(opened.buffer->text() == "one\ntwo\nthree\nfour", "opened text normalization");
    require(opened.metadata.mixedLineEndings, "mixed ending detection");
    require(opened.buffer->snapshot().lineEndings == std::vector<LineEnding>{LineEnding::crlf,
            LineEnding::cr, LineEnding::lf}, "mixed endings preserved in buffer");
    DocumentManager manager;
    auto& document = manager.registerOpened(std::move(opened.buffer), std::move(opened.metadata));
    require(!document.modified() && document.metadata().path.has_value(), "opened document saved state");
    require(document.metadata().file && document.metadata().file->byteSize ==
            static_cast<std::uintmax_t>(mixed.size()), "file metadata");
    require(manager.find(document.id()) == &document && manager.activeDocument() == document.id(), "manager lookup/active");
    require(static_cast<bool>(service.save(document)) && readBytes(source) == mixed, "mixed ending save round trip");

    auto duplicate = service.open(source);
    bool duplicateRejected = false;
    try { (void)manager.registerOpened(std::move(duplicate.buffer), std::move(duplicate.metadata)); }
    catch (const std::invalid_argument&) { duplicateRejected = true; }
    require(duplicateRejected && manager.size() == 1, "duplicate path registration");

    document.buffer().insert(document.buffer().sizeBytes(), "\nlast");
    require(document.modified(), "buffer revision marks modified");
    const auto failedPath = root / "missing-parent" / "output.txt";
    const auto failedSave = service.saveAs(document, failedPath);
    require(!failedSave && document.modified(), "failed save must preserve modified state");
    require(document.metadata().path == source, "failed Save As must not change associated path");
    require(readBytes(source) == mixed, "failed Save As must not alter original file");
    const auto blockedTarget = root / "blocked-target";
    std::filesystem::create_directory(blockedTarget);
    const auto marker = blockedTarget / "keep.txt";
    writeBytes(marker, QByteArray("keep", 4));
    const auto commitFailure = service.saveAs(document, blockedTarget);
    require(!commitFailure && readBytes(marker) == QByteArray("keep", 4) && readBytes(source) == mixed,
            "failed atomic replacement must preserve existing data");
    require(manager.close(document.id()) == CloseResult::confirmationRequired, "modified close confirmation");
    require(manager.close(document.id(), true) == CloseResult::closed && manager.size() == 0, "explicit discard close");

    auto fresh = manager.createNew();
    require(!fresh.metadata().path && !fresh.modified(), "new untitled document");
    fresh.buffer().insert(0, "saved\r\ntext");
    const auto savePath = root / "saved.txt";
    require(static_cast<bool>(service.saveAs(fresh, savePath)), "Save As success");
    require(!fresh.modified() && fresh.metadata().path == savePath, "successful Save As state");
    fresh.buffer().replace(0, 5, "other");
    require(fresh.modified(), "edit after save marks modified");
    require(static_cast<bool>(service.save(fresh)), "save existing document");
    require(!fresh.modified() && readBytes(savePath) == QByteArray("other\r\ntext", 11), "save contents and EOL");

    const auto utf8BomPath = root / "utf8bom.txt";
    const QByteArray utf8Bom = QByteArray::fromHex("efbbbf") + QString::fromUtf8("café\r\n🙂").toUtf8();
    writeBytes(utf8BomPath, utf8Bom);
    auto bom = service.open(utf8BomPath);
    require(bom && bom.metadata.encoding == FileEncoding::utf8Bom && bom.buffer->text() == "café\n🙂", "UTF-8 BOM decode");
    auto& bomDoc = manager.registerOpened(std::move(bom.buffer), std::move(bom.metadata));
    require(service.save(bomDoc) && readBytes(utf8BomPath) == utf8Bom, "UTF-8 BOM round trip");

    const auto utf16LePath = root / "utf16le.txt";
    const QByteArray utf16Le = QByteArray::fromHex("fffe41000d000a00a9033dd842de");
    writeBytes(utf16LePath, utf16Le);
    auto le = service.open(utf16LePath);
    require(le && le.metadata.encoding == FileEncoding::utf16LittleEndian && le.buffer->text() == "A\nΩ🙂", "UTF-16 LE decode");
    auto& leDoc = manager.registerOpened(std::move(le.buffer), std::move(le.metadata));
    require(service.save(leDoc) && readBytes(utf16LePath) == utf16Le, "UTF-16 LE round trip");

    const auto utf16BePath = root / "utf16be.txt";
    const QByteArray utf16Be = QByteArray::fromHex("feff0041000d000a03a9d83dde42");
    writeBytes(utf16BePath, utf16Be);
    auto be = service.open(utf16BePath);
    require(be && be.metadata.encoding == FileEncoding::utf16BigEndian && be.buffer->text() == "A\nΩ🙂", "UTF-16 BE decode");
    auto& beDoc = manager.registerOpened(std::move(be.buffer), std::move(be.metadata));
    require(service.save(beDoc) && readBytes(utf16BePath) == utf16Be, "UTF-16 BE round trip");

    const auto malformedPath = root / "bad.txt";
    writeBytes(malformedPath, QByteArray::fromHex("c328"));
    const auto malformed = service.open(malformedPath);
    require(!malformed && !malformed.error.empty(), "malformed UTF-8 must report error");
    const auto missing = service.open(root / "absent.txt");
    require(!missing && !missing.error.empty(), "read failure must report error");

    const auto emptyPath = root / "empty.txt";
    writeBytes(emptyPath, {});
    auto empty = service.open(emptyPath);
    require(empty && empty.buffer->lineCount() == 1 && empty.buffer->lineText(0).empty(), "empty file open");
    auto& emptyDoc = manager.registerOpened(std::move(empty.buffer), std::move(empty.metadata));
    require(static_cast<bool>(service.save(emptyDoc)) && readBytes(emptyPath).isEmpty(), "empty file round trip");

    const auto malformed16Path = root / "bad16.txt";
    writeBytes(malformed16Path, QByteArray::fromHex("fffe4100ff"));
    const auto malformed16 = service.open(malformed16Path);
    require(!malformed16 && !malformed16.error.empty(), "malformed UTF-16 must report error");

}

void testChunkedFileOpen() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    const auto path = std::filesystem::path(temporary.filePath(QStringLiteral("chunked.txt")).toStdWString());
    writeBytes(path, QByteArray::fromHex("efbbbf610d0a62f09f98800d6364"));
    note::documents::FileService service;
    note::documents::OpenOptions options;
    options.chunkBytes = 5;
    std::size_t progressCount = 0;
    options.progress = [&](note::documents::OpenProgress progress) {
        if (!progress.buildingIndex) ++progressCount;
    };
    const auto opened = service.open(path, options);
    require(opened && opened.buffer->text() == "a\nb\xf0\x9f\x98\x80\ncd",
            "streaming decoder preserves split BOM, UTF-8 scalar, and CR/LF boundaries");
    require(opened.buffer->lineEnding(0) == LineEnding::crlf &&
            opened.buffer->lineEnding(1) == LineEnding::cr,
            "streaming line ending normalization preserves original forms");
    require(progressCount >= 2, "file loading reports incremental byte progress");

    const auto utf16Path = std::filesystem::path(temporary.filePath(QStringLiteral("split16.txt")).toStdWString());
    writeBytes(utf16Path, QByteArray::fromHex("fffe41000d000a003dd842de"));
    note::documents::OpenOptions utf16Options;
    utf16Options.chunkBytes = 1;
    const auto utf16 = service.open(utf16Path, utf16Options);
    const std::string expectedUtf16Text = std::string("A\n") + std::string("\xf0\x9f\x99\x82", 4);
    require(utf16 && utf16.buffer->text() == expectedUtf16Text &&
            utf16.buffer->lineEnding(0) == LineEnding::crlf,
            "streaming UTF-16 decoder preserves split CRLF and surrogate boundaries");
    const auto unmatchedSurrogate = std::filesystem::path(temporary.filePath(QStringLiteral("bad_surrogate.txt")).toStdWString());
    writeBytes(unmatchedSurrogate, QByteArray::fromHex("fffe00d8"));
    require(!service.open(unmatchedSurrogate, utf16Options), "unmatched UTF-16 surrogate must be rejected");

    const auto malformedPath = std::filesystem::path(temporary.filePath(QStringLiteral("truncated.txt")).toStdWString());
    writeBytes(malformedPath, QByteArray::fromHex("f09f"));
    require(!service.open(malformedPath), "truncated UTF-8 must be rejected at final buffer validation");

    const auto cancelPath = std::filesystem::path(temporary.filePath(QStringLiteral("cancel.txt")).toStdWString());
    writeBytes(cancelPath, QByteArray(1024 * 1024, 'x'));
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    options.cancelled = cancelled;
    options.progress = [cancelled](note::documents::OpenProgress progress) {
        if (progress.bytesRead >= 5) cancelled->store(true);
    };
    const auto stopped = service.open(cancelPath, options);
    require(!stopped && stopped.error == "Opening was cancelled.", "streaming file read observes cancellation between chunks");
}

} // namespace

int main() {
    try {
        testTextBuffer();
        testDocumentsAndFiles();
        testChunkedFileOpen();
        std::cout << "All document foundation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Document foundation test failure: " << error.what() << '\n';
        return 1;
    }
}
