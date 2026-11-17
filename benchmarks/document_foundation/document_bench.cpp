#include "editor/text_buffer.hpp"
#include "documents/document_manager.hpp"
#include "documents/file_service.hpp"
#include "text_buffers.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono;

double millis(Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(p * static_cast<double>(values.size() - 1));
    return values[index];
}

template<typename Buffer>
std::size_t bufferSize(const Buffer& buffer) {
    if constexpr (std::is_same_v<Buffer, note::editor::TextBuffer>) return buffer.sizeBytes();
    else return buffer.size();
}

std::string fixture(std::size_t bytes) {
    constexpr std::string_view line = "int value_0123456789 = 42; /* deterministic benchmark row */\n";
    std::string text;
    text.reserve(bytes);
    while (text.size() + line.size() <= bytes) text.append(line);
    text.append(bytes - text.size(), 'x');
    return text;
}

template<typename Buffer>
void runBuffer(const std::string& name, std::string_view input, std::uint32_t seed) {
    std::unique_ptr<Buffer> buffer;
    const auto loadStart = Clock::now();
    if constexpr (std::is_same_v<Buffer, note::editor::TextBuffer>) {
        buffer = std::make_unique<Buffer>(input);
    } else {
        buffer = std::make_unique<Buffer>(input.size());
        buffer->appendOriginal(input);
    }
    const auto loadMs = millis(Clock::now() - loadStart);

    const auto typingPosition = bufferSize(*buffer) / 2;
    std::vector<double> typing;
    auto cursor = typingPosition;
    for (int i = 0; i < 200; ++i) {
        const auto start = Clock::now();
        buffer->insert(cursor, "x");
        typing.push_back(millis(Clock::now() - start));
        ++cursor;
    }
    buffer->erase(typingPosition, 200);

    std::vector<double> local;
    const auto middle = bufferSize(*buffer) / 2;
    for (int i = 0; i < 200; ++i) {
        const auto start = Clock::now();
        buffer->insert(middle, "x");
        buffer->erase(middle, 1);
        local.push_back(millis(Clock::now() - start));
    }

    const std::string paste(256 * 1024, 'p');
    const auto pasteAt = Clock::now();
    const auto pastePosition = bufferSize(*buffer) / 2;
    buffer->insert(pastePosition, paste);
    buffer->erase(pastePosition, paste.size());
    const auto pasteMs = millis(Clock::now() - pasteAt);

    std::mt19937 random(seed);
    std::vector<double> randomInsert;
    std::vector<std::size_t> positions;
    positions.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        const auto position = static_cast<std::size_t>(random()) % (bufferSize(*buffer) + 1);
        const auto start = Clock::now();
        buffer->insert(position, "z");
        randomInsert.push_back(millis(Clock::now() - start));
        positions.push_back(position);
    }
    std::vector<double> deletion;
    for (auto position : positions) {
        position = std::min(position, bufferSize(*buffer) - 1);
        const auto start = Clock::now();
        buffer->erase(position, 1);
        deletion.push_back(millis(Clock::now() - start));
    }

    std::vector<double> retrieval;
    const auto retrievalSeed = seed ^ 0x5a5a5a5aU;
    std::mt19937 retrievalRandom(retrievalSeed);
    std::size_t retrievedBytes = 0;
    for (int i = 0; i < 100; ++i) {
        const auto maxStart = bufferSize(*buffer) - 64 * 1024;
        const auto position = static_cast<std::size_t>(retrievalRandom()) % (maxStart + 1);
        const auto start = Clock::now();
        if constexpr (std::is_same_v<Buffer, note::editor::TextBuffer>)
            retrievedBytes += buffer->textRange(position, 64 * 1024).size();
        else
            retrievedBytes += buffer->copy(position, 64 * 1024).size();
        retrieval.push_back(millis(Clock::now() - start));
    }

    std::cout << "  " << name << ": load=" << std::fixed << std::setprecision(3) << loadMs
              << " ms, storage~=";
    if constexpr (std::is_same_v<Buffer, note::editor::TextBuffer>)
        std::cout << buffer->estimatedStorageBytes() / (1024.0 * 1024.0) << " MiB, pieces=" << buffer->pieceCount();
    else
        std::cout << buffer->allocatedBytes() / (1024.0 * 1024.0) << " MiB";
    std::cout << ", local insert/delete p95=" << percentile(local, .95) << " ms"
              << ", sequential typing p95=" << percentile(typing, .95) << " ms"
              << ", 256 KiB paste+delete=" << pasteMs << " ms"
              << ", random insert p95=" << percentile(randomInsert, .95) << " ms"
              << ", random delete p95=" << percentile(deletion, .95) << " ms"
              << ", 64 KiB retrieval p95=" << percentile(retrieval, .95) << " ms"
              << " (" << retrievedBytes / 1024 << " KiB read)";

    if constexpr (std::is_same_v<Buffer, note::editor::TextBuffer>) {
        std::vector<double> lineLookup;
        std::mt19937 lineRandom(seed ^ 0xa5a5a5a5U);
        const auto lineCount = buffer->lineCount();
        for (int i = 0; i < 1000; ++i) {
            const auto line = static_cast<std::size_t>(lineRandom()) % lineCount;
            const auto start = Clock::now();
            const auto range = buffer->lineRange(line);
            (void)range;
            lineLookup.push_back(millis(Clock::now() - start));
        }
        std::cout << ", lines=" << lineCount << ", line lookup p95=" << percentile(lineLookup, .95) << " ms";
    }
    std::cout << '\n';
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

QString toQString(const std::filesystem::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    const auto bytes = path.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
#endif
}

void runFileService(std::string_view text, QTemporaryDir& temporary) {
    const auto inputPath = pathFromQString(temporary.filePath("input.txt"));
    const auto outputPath = pathFromQString(temporary.filePath("output.txt"));
    QFile file(toQString(inputPath));
    if (!file.open(QIODevice::WriteOnly) || file.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size())) {
        std::cout << "  FileService: fixture write failed\n";
        return;
    }
    file.close();

    note::documents::FileService service;
    const auto openStart = Clock::now();
    auto opened = service.open(inputPath);
    const auto openMs = millis(Clock::now() - openStart);
    if (!opened) {
        std::cout << "  FileService: open failed: " << opened.error << '\n';
        return;
    }
    note::documents::DocumentManager manager;
    auto& document = manager.registerOpened(std::move(opened.buffer), std::move(opened.metadata));
    const auto saveStart = Clock::now();
    const auto saved = service.saveAs(document, outputPath);
    const auto saveMs = millis(Clock::now() - saveStart);
    std::cout << "  FileService: open=" << openMs << " ms, atomic Save As=" << saveMs
              << " ms, success=" << static_cast<bool>(saved) << '\n';
}

std::size_t parseSize(const char* text) {
    return static_cast<std::size_t>(std::stoull(text)) * 1024 * 1024;
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    std::vector<std::size_t> sizes;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) sizes.push_back(parseSize(argv[i]));
    } else {
        sizes = {1, 10, 50, 100};
        for (auto& size : sizes) size *= 1024 * 1024;
    }
    std::cout << "NOTE document foundation benchmark; release build, deterministic LF fixture.\n";
    for (const auto size : sizes) {
        auto input = fixture(size);
        std::cout << "Fixture " << size / (1024 * 1024) << " MiB, bytes=" << input.size() << '\n';
        const auto seed = static_cast<std::uint32_t>(size ^ 0x4e4f5445U);
        runBuffer<note::prototype::GapBuffer>("flat gap", input, seed);
        runBuffer<note::prototype::PieceTable>("flat piece table", input, seed);
        runBuffer<note::editor::TextBuffer>("indexed piece treap", input, seed);
        QTemporaryDir temporary;
        runFileService(input, temporary);
    }
    std::string longLine(8 * 1024 * 1024, 'x');
    std::cout << "Fixture 8 MiB single long line, bytes=" << longLine.size() << '\n';
    const auto longSeed = 0x4c4f4e47U;
    runBuffer<note::prototype::GapBuffer>("flat gap", longLine, longSeed);
    runBuffer<note::prototype::PieceTable>("flat piece table", longLine, longSeed);
    runBuffer<note::editor::TextBuffer>("indexed piece treap", longLine, longSeed);
    return 0;
}
