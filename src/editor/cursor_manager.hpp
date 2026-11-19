#pragma once

#include <cstddef>
#include <vector>

namespace note::editor {

class TextBuffer;

struct CursorLocation {
    std::size_t line{};
    std::size_t column{};
};

class CursorManager final {
public:
    explicit CursorManager(const TextBuffer& buffer);

    [[nodiscard]] std::size_t primaryOffset() const noexcept;
    [[nodiscard]] std::size_t primaryCursor() const noexcept;
    [[nodiscard]] CursorLocation primaryLocation() const;
    void setPrimaryCursor(std::size_t offset);
    void setPrimaryPosition(std::size_t offset);
    void setCursor(std::size_t offset);
    void moveToOffset(std::size_t offset);

    [[nodiscard]] std::size_t cursorCount() const noexcept;
    [[nodiscard]] std::size_t secondaryCursorCount() const noexcept;
    [[nodiscard]] const std::vector<std::size_t>& secondaryCursorOffsets() const noexcept;
    [[nodiscard]] std::size_t secondaryCursorOffset(std::size_t index) const;
    bool addSecondaryCursor(std::size_t offset);
    void setSecondaryCursor(std::size_t index, std::size_t offset);
    bool removeSecondaryCursor(std::size_t index);
    void clearSecondaryCursors();

    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void moveToDocumentStart();
    void moveToDocumentEnd();
    void moveToLineColumn(std::size_t line, std::size_t column);

    [[nodiscard]] std::size_t lineCount() const;
    [[nodiscard]] CursorLocation lineAndColumn(std::size_t offset) const;
    [[nodiscard]] std::size_t clampOffset(std::size_t offset) const;
    void syncToBuffer();

    // Adjust cursor state after a completed TextBuffer replacement.
    // This method does not modify document text.
    void onTextReplaced(std::size_t offset, std::size_t removedBytes,
                        std::size_t insertedBytes);

private:
    struct CursorState {
        std::size_t offset{};
        std::size_t preferredColumn{};
    };

    [[nodiscard]] std::size_t clampWithinScalar(std::size_t offset) const;
    [[nodiscard]] std::size_t clampLineColumn(std::size_t line, std::size_t column) const;
    [[nodiscard]] std::size_t moveOffsetLeft(std::size_t offset) const;
    [[nodiscard]] std::size_t moveOffsetRight(std::size_t offset) const;
    void syncState(CursorState& cursor);

    const TextBuffer& buffer_;
    CursorState primary_;
    std::vector<CursorState> secondary_;
    mutable std::vector<std::size_t> secondaryOffsets_;
};

} // namespace note::editor
