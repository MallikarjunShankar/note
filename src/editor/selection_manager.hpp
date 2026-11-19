#pragma once

#include <cstddef>
#include <vector>

namespace note::editor {

class TextBuffer;

struct SelectionRange {
    std::size_t begin{};
    std::size_t end{};
};

struct Selection {
    std::size_t anchor{};
    std::size_t active{};
};

class SelectionManager final {
public:
    explicit SelectionManager(const TextBuffer& buffer);

    [[nodiscard]] std::size_t selectionCount() const noexcept;
    [[nodiscard]] const Selection& selection(std::size_t index) const;
    [[nodiscard]] SelectionRange normalizedRange(std::size_t index) const;
    [[nodiscard]] bool hasSelection(std::size_t index) const;

    void setSelection(std::size_t index, std::size_t anchor, std::size_t active);
    std::size_t addSelection(std::size_t anchor, std::size_t active);
    bool removeSelection(std::size_t index);
    void setAnchor(std::size_t index, std::size_t offset);
    void setActive(std::size_t index, std::size_t offset);
    void clearSelection(std::size_t index);
    void clearSelections() noexcept;
    void syncToBuffer();

    // Apply the same endpoint transform as a completed TextBuffer replacement.
    // Call this after the buffer has been changed. Invalid edit coordinates
    // are rejected instead of being guessed or silently repaired.
    void onTextReplaced(std::size_t offset, std::size_t removedBytes,
                        std::size_t insertedBytes);

private:
    [[nodiscard]] std::size_t clampOffset(std::size_t offset) const;
    [[nodiscard]] Selection& selectionAt(std::size_t index);

    const TextBuffer& buffer_;
    std::vector<Selection> selections_;
};

} // namespace note::editor
