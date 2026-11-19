#pragma once

#include "editor/text_buffer.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace note::editor {

class CursorManager;
class SelectionManager;

struct EditRecord {
    std::size_t offset{};
    std::string removedText;
    std::string insertedText;
    TextSnapshot removedSnapshot;
    TextSnapshot insertedSnapshot;
};

using EditBatch = std::vector<EditRecord>;

class EditingEngine final {
public:
    EditingEngine(TextBuffer& buffer, CursorManager& cursors,
                  SelectionManager& selections);

    [[nodiscard]] EditBatch insertText(
        std::string_view utf8Text, LineEnding bareLineEnding = LineEnding::lf);
    [[nodiscard]] EditBatch backspace();
    [[nodiscard]] EditBatch deleteForward();
    [[nodiscard]] EditBatch insertNewline(
        LineEnding lineEnding = LineEnding::lf);
    [[nodiscard]] EditBatch replaceSelections(
        std::string_view utf8Text, LineEnding bareLineEnding = LineEnding::lf);
    [[nodiscard]] EditBatch deleteSelections();

private:
    struct PendingEdit {
        std::size_t offset{};
        std::size_t length{};
        std::string insertedText;
    };

    [[nodiscard]] EditBatch apply(
        std::vector<PendingEdit> edits, LineEnding bareLineEnding,
        bool clearSelections);
    [[nodiscard]] std::size_t previousBoundary(std::size_t offset) const;
    [[nodiscard]] std::size_t nextBoundary(std::size_t offset) const;
    void validateInsertedText(std::string_view utf8Text) const;
    void validateEdits(const std::vector<PendingEdit>& edits) const;
    void rejectOverlaps(const std::vector<PendingEdit>& edits) const;

    TextBuffer& buffer_;
    CursorManager& cursors_;
    SelectionManager& selections_;
};

} // namespace note::editor
