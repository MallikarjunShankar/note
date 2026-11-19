#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace note::editor {

enum class LineEnding : std::uint8_t { lf, crlf, cr };

struct LineRange {
    std::size_t begin{};
    std::size_t end{}; // Excludes the line-feed separator.
};

struct TextSnapshot {
    std::string utf8; // Canonical LF separators.
    std::vector<LineEnding> lineEndings; // One entry per LF in utf8.
};

// Offsets and lengths are UTF-8 byte coordinates. Public edit boundaries must
// also be Unicode scalar boundaries. Line separators are canonical LF bytes;
// their original LF/CRLF/CR forms are retained as metadata for lossless saves.
class TextBuffer final {
public:
    TextBuffer();
    explicit TextBuffer(std::string_view utf8Text);
    explicit TextBuffer(TextSnapshot canonicalText);
    ~TextBuffer();
    TextBuffer(TextBuffer&&) noexcept;
    TextBuffer& operator=(TextBuffer&&) noexcept;
    TextBuffer(const TextBuffer&) = delete;
    TextBuffer& operator=(const TextBuffer&) = delete;

    [[nodiscard]] std::size_t sizeBytes() const noexcept;
    [[nodiscard]] std::size_t lineCount() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::size_t pieceCount() const noexcept;
    [[nodiscard]] std::size_t estimatedStorageBytes() const noexcept;

    [[nodiscard]] std::string text() const;
    [[nodiscard]] std::string textRange(std::size_t offset, std::size_t length) const;
    [[nodiscard]] LineRange lineRange(std::size_t line) const;
    [[nodiscard]] std::string lineText(std::size_t line) const;
    [[nodiscard]] bool isScalarBoundary(std::size_t offset) const;
    [[nodiscard]] unsigned char byteAt(std::size_t offset) const;
    [[nodiscard]] LineEnding lineEnding(std::size_t newlineIndex) const;
    [[nodiscard]] TextSnapshot snapshot() const;

    void insert(std::size_t offset, std::string_view utf8Text,
                LineEnding bareLineEnding = LineEnding::lf);
    void insert(std::size_t offset, const TextSnapshot& snapshot);
    void erase(std::size_t offset, std::size_t length);
    void replace(std::size_t offset, std::size_t length, std::string_view utf8Text,
                 LineEnding bareLineEnding = LineEnding::lf);
    void replace(std::size_t offset, std::size_t length,
                 const TextSnapshot& snapshot);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace note::editor
