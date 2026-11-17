#include "text_buffers.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace note::prototype;

template<class F>
double milliseconds(F&& function) {
    const auto start = Clock::now();
    function();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void selfTest() {
    for (const char* kind : {"gap", "piece"}) {
        auto buffer = makeBuffer(kind, 128);
        buffer->appendOriginal("alpha\nbeta\ngamma");
        require(buffer->copy(0, buffer->size()) == "alpha\nbeta\ngamma", "initial content");
        buffer->insert(6, "INSERT");
        require(buffer->copy(0, buffer->size()) == "alpha\nINSERTbeta\ngamma", "insert at offset");
        buffer->erase(6, 6);
        require(buffer->copy(0, buffer->size()) == "alpha\nbeta\ngamma", "delete at offset");
        buffer->insert(buffer->size(), "!");
        require(buffer->at(buffer->size() - 1) == '!', "append");
        buffer->erase(buffer->size() - 1, 1);
        require(buffer->copy(3, 7) == "ha\nbeta", "range copy across boundary");

        std::string expected = buffer->copy(0, buffer->size());
        std::mt19937 random(0x4e4f5445);
        for (int operation = 0; operation < 2000; ++operation) {
            if (expected.empty() || random() % 3 != 0) {
                const auto position = static_cast<std::size_t>(random() % (expected.size() + 1));
                const std::string text(1 + random() % 4, static_cast<char>('a' + random() % 26));
                buffer->insert(position, text);
                expected.insert(position, text);
            } else {
                const auto position = static_cast<std::size_t>(random() % expected.size());
                const auto length = std::min<std::size_t>(1 + random() % 5, expected.size() - position);
                buffer->erase(position, length);
                expected.erase(position, length);
            }
            require(buffer->size() == expected.size(), "randomized size");
            require(buffer->copy(0, buffer->size()) == expected, "randomized content");
        }
    }
    std::cout << "buffer self-test: PASS (gap, piece, randomized edits, insert, erase, copy)\n";
}

std::filesystem::path generateFixture(std::uint64_t bytes) {
    const auto path = std::filesystem::temp_directory_path() / "note_editor_50mb_fixture.txt";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot create temporary fixture");
    constexpr std::string_view line = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";
    std::uint64_t written = 0;
    while (written < bytes) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(line.size(), bytes - written));
        out.write(line.data(), static_cast<std::streamsize>(count));
        written += count;
    }
    out.close();
    return path;
}

void benchmark(const std::filesystem::path& path) {
    const auto fileBytes = std::filesystem::file_size(path);
    constexpr std::size_t chunkSize = 1024 * 1024;
    std::vector<char> chunk(chunkSize);
    for (const char* kind : {"gap", "piece"}) {
        std::mt19937_64 rng(0x4e4f5445);
        auto buffer = makeBuffer(kind, static_cast<std::size_t>(fileBytes));
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("cannot read fixture");
        const auto loadMs = milliseconds([&] {
            while (in) {
                in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                const auto count = static_cast<std::size_t>(in.gcount());
                if (count) buffer->appendOriginal(std::string_view(chunk.data(), count));
            }
        });
        if (buffer->size() != fileBytes) throw std::runtime_error("fixture load length mismatch");

        const auto center = buffer->size() / 2;
        const auto initialCursorMoveUs = milliseconds([&] {
            buffer->insert(center, "x");
            buffer->erase(center, 1);
        }) * 1000.0;
        const auto localRoundTripNs = milliseconds([&] {
            for (int i = 0; i < 10000; ++i) {
                buffer->insert(center, "x");
                buffer->erase(center, 1);
            }
        }) * 1.0e6 / 10000.0;

        const auto burstMs = milliseconds([&] {
            for (int i = 0; i < 500; ++i) buffer->insert(center + static_cast<std::size_t>(i), "x");
            buffer->erase(center, 500);
        });
        const auto allocatedAfterLoad = buffer->allocatedBytes();

        std::vector<double> randomEdit;
        randomEdit.reserve(1000);
        const auto randomInsertMs = milliseconds([&] {
          for (int i = 0; i < 1000; ++i) {
            const auto position = static_cast<std::size_t>(rng() % (buffer->size() - 1));
            randomEdit.push_back(milliseconds([&] { buffer->insert(position, "y"); }));
          }
        });
        std::sort(randomEdit.begin(), randomEdit.end());

        double viewportReadMs = 0;
        std::uint64_t checksum = 0;
        viewportReadMs = milliseconds([&] {
            for (int sweep = 0; sweep < 1000; ++sweep) {
                const auto position = static_cast<std::size_t>((rng() % (buffer->size() - 256)));
                const auto window = buffer->copy(position, 256);
                checksum += static_cast<unsigned char>(window[sweep % window.size()]);
            }
        });

        std::cout << "buffer=" << kind
                  << " bytes=" << buffer->size()
                  << " allocated_after_load_MiB=" << (static_cast<double>(allocatedAfterLoad) / (1024.0 * 1024.0))
                  << " allocated_after_edits_MiB=" << (static_cast<double>(buffer->allocatedBytes()) / (1024.0 * 1024.0))
                  << " pieces=" << (kind == std::string_view("piece")
                                      ? static_cast<PieceTable&>(*buffer).pieceCount() : 1)
                  << " load_ms=" << loadMs
                  << " first_center_edit_us=" << initialCursorMoveUs
                  << " local_10000_insert_delete_ns_per_pair=" << localRoundTripNs
                  << " sequential_500_insert_delete_ms=" << burstMs
                  << " random_1000_insert_total_ms=" << randomInsertMs
                  << " random_insert_median_us=" << randomEdit[500] * 1000.0
                  << " random_insert_p95_us=" << randomEdit[949] * 1000.0
                  << " random_insert_max_us=" << randomEdit.back() * 1000.0
                  << " viewport_1000x256_ms=" << viewportReadMs
                  << " checksum=" << checksum << '\n';
    }
    std::cout << "fixture=" << path.string() << " bytes=" << fileBytes
              << " size_MiB=" << (static_cast<double>(fileBytes) / (1024.0 * 1024.0)) << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--self-test") {
            selfTest();
            return 0;
        }
        const auto path = argc > 1 ? std::filesystem::path(argv[1]) : generateFixture(50ULL * 1024ULL * 1024ULL);
        benchmark(path);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "buffer_bench error: " << error.what() << '\n';
        return 1;
    }
}
