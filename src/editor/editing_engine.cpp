#include "editor/editing_engine.hpp"

#include "editor/cursor_manager.hpp"
#include "editor/selection_manager.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace note::editor {

EditingEngine::EditingEngine(TextBuffer& buffer, CursorManager& cursors,
                             SelectionManager& selections)
    : buffer_(buffer), cursors_(cursors), selections_(selections) {}

EditBatch EditingEngine::insertText(std::string_view utf8Text,
                                    LineEnding bareLineEnding) {
    validateInsertedText(utf8Text);
    std::vector<PendingEdit> edits;
    edits.reserve(cursors_.cursorCount());
    for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
        const auto offset = index == 0 ? cursors_.primaryOffset()
                                      : cursors_.secondaryCursorOffset(index - 1);
        edits.push_back({offset, 0, std::string(utf8Text)});
    }
    return apply(std::move(edits), bareLineEnding, false);
}

EditBatch EditingEngine::insertTextAtCursorIndices(
    std::span<const std::size_t> cursorIndices, std::string_view utf8Text,
    LineEnding bareLineEnding) {
    validateInsertedText(utf8Text);
    std::vector<bool> seen(cursors_.cursorCount());
    std::vector<PendingEdit> edits;
    edits.reserve(cursorIndices.size());
    for (const auto index : cursorIndices) {
        if (index >= cursors_.cursorCount() || seen[index])
            throw std::invalid_argument("target cursor index is invalid or duplicated");
        seen[index] = true;
        const auto offset = index == 0 ? cursors_.primaryOffset()
                                       : cursors_.secondaryCursorOffset(index - 1);
        edits.push_back({offset, 0, std::string(utf8Text)});
    }
    return apply(std::move(edits), bareLineEnding, false);
}

EditBatch EditingEngine::backspace() {
    std::vector<PendingEdit> edits;
    edits.reserve(cursors_.cursorCount());
    bool selectedRange = false;
    for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
        if (index < selections_.selectionCount() && selections_.hasSelection(index)) {
            const auto range = selections_.normalizedRange(index);
            edits.push_back({range.begin, range.end - range.begin, {}});
            selectedRange = true;
            continue;
        }
        const auto cursor = index == 0 ? cursors_.primaryOffset()
                                       : cursors_.secondaryCursorOffset(index - 1);
        if (cursor != 0) {
            const auto begin = previousBoundary(cursor);
            edits.push_back({begin, cursor - begin, {}});
        }
    }
    return apply(std::move(edits), LineEnding::lf, selectedRange);
}

EditBatch EditingEngine::deleteForward() {
    std::vector<PendingEdit> edits;
    edits.reserve(cursors_.cursorCount());
    bool selectedRange = false;
    for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
        if (index < selections_.selectionCount() && selections_.hasSelection(index)) {
            const auto range = selections_.normalizedRange(index);
            edits.push_back({range.begin, range.end - range.begin, {}});
            selectedRange = true;
            continue;
        }
        const auto cursor = index == 0 ? cursors_.primaryOffset()
                                       : cursors_.secondaryCursorOffset(index - 1);
        if (cursor != buffer_.sizeBytes()) {
            const auto end = nextBoundary(cursor);
            edits.push_back({cursor, end - cursor, {}});
        }
    }
    return apply(std::move(edits), LineEnding::lf, selectedRange);
}

EditBatch EditingEngine::insertNewline(LineEnding lineEnding) {
    return insertText("\n", lineEnding);
}

EditBatch EditingEngine::replaceSelections(std::string_view utf8Text,
                                           LineEnding bareLineEnding) {
    validateInsertedText(utf8Text);
    std::vector<PendingEdit> edits;
    edits.reserve(selections_.selectionCount());
    for (std::size_t index = 0; index < selections_.selectionCount(); ++index) {
        const auto range = selections_.normalizedRange(index);
        edits.push_back({range.begin, range.end - range.begin,
                         std::string(utf8Text)});
    }
    auto result = apply(std::move(edits), bareLineEnding, false);
    for (std::size_t index = 0; index < selections_.selectionCount() &&
                                index < cursors_.cursorCount(); ++index) {
        const auto endpoint = selections_.normalizedRange(index).end;
        if (index == 0) cursors_.setPrimaryCursor(endpoint);
        else cursors_.setSecondaryCursor(index - 1, endpoint);
    }
    selections_.clearSelections();
    return result;
}

EditBatch EditingEngine::deleteSelections() {
    std::vector<PendingEdit> edits;
    edits.reserve(selections_.selectionCount());
    for (std::size_t index = 0; index < selections_.selectionCount(); ++index) {
        const auto range = selections_.normalizedRange(index);
        if (range.begin != range.end)
            edits.push_back({range.begin, range.end - range.begin, {}});
    }
    auto result = apply(std::move(edits), LineEnding::lf, false);
    for (std::size_t index = 0; index < selections_.selectionCount() &&
                                index < cursors_.cursorCount(); ++index) {
        const auto endpoint = selections_.normalizedRange(index).end;
        if (index == 0) cursors_.setPrimaryCursor(endpoint);
        else cursors_.setSecondaryCursor(index - 1, endpoint);
    }
    selections_.clearSelections();
    return result;
}

EditBatch EditingEngine::replaceRange(std::size_t offset, std::size_t length,
                                      std::string_view utf8Text,
                                      LineEnding bareLineEnding) {
    validateInsertedText(utf8Text);
    std::vector<PendingEdit> edits;
    edits.push_back({offset, length, std::string(utf8Text)});
    return apply(std::move(edits), bareLineEnding, false);
}

std::size_t EditingEngine::previousBoundary(std::size_t offset) const {
    auto result = offset - 1;
    while (result > 0 && !buffer_.isScalarBoundary(result)) --result;
    return result;
}

std::size_t EditingEngine::nextBoundary(std::size_t offset) const {
    auto result = offset + 1;
    while (result < buffer_.sizeBytes() && !buffer_.isScalarBoundary(result))
        ++result;
    return result;
}

void EditingEngine::validateInsertedText(std::string_view utf8Text) const {
    TextBuffer candidate(utf8Text);
    (void)candidate;
}

void EditingEngine::validateEdits(const std::vector<PendingEdit>& edits) const {
    for (const auto& edit : edits) {
        if (edit.offset > buffer_.sizeBytes() ||
            edit.length > buffer_.sizeBytes() - edit.offset ||
            !buffer_.isScalarBoundary(edit.offset) ||
            !buffer_.isScalarBoundary(edit.offset + edit.length)) {
            throw std::invalid_argument("editing operation has an invalid range");
        }
    }
}

void EditingEngine::rejectOverlaps(const std::vector<PendingEdit>& edits) const {
    auto ordered = edits;
    std::sort(ordered.begin(), ordered.end(), [](const PendingEdit& left,
                                                 const PendingEdit& right) {
        if (left.offset != right.offset) return left.offset < right.offset;
        return left.length < right.length;
    });
    for (std::size_t index = 1; index < ordered.size(); ++index) {
        const auto& previous = ordered[index - 1];
        const auto& current = ordered[index];
        const auto previousEnd = previous.offset + previous.length;
        const bool pointConflict = previous.length == 0 && current.length == 0 &&
                                   previous.offset == current.offset;
        const bool rangeConflict = previous.length != 0 && current.offset < previousEnd;
        const bool pointAtRangeStart = previous.length == 0 && current.length != 0 &&
                                       current.offset == previous.offset;
        const bool pointInRange = current.length == 0 && current.offset <= previousEnd &&
                                  current.offset >= previous.offset &&
                                  previous.length != 0;
        if (pointConflict || rangeConflict || pointAtRangeStart || pointInRange)
            throw std::invalid_argument("editing operation has overlapping edits");
    }
}

EditBatch EditingEngine::apply(std::vector<PendingEdit> edits,
                               LineEnding bareLineEnding, bool clearSelections) {
    validateEdits(edits);
    rejectOverlaps(edits);
    std::sort(edits.begin(), edits.end(), [](const PendingEdit& left,
                                             const PendingEdit& right) {
        return left.offset > right.offset;
    });

    EditBatch records;
    records.reserve(edits.size());
    for (const auto& edit : edits) {
        const auto removedText = buffer_.textRange(edit.offset, edit.length);
        TextSnapshot removedSnapshot{removedText, {}};
        for (std::size_t newline = 0; newline < buffer_.lineCount(); ++newline) {
            const auto range = buffer_.lineRange(newline);
            if (range.end >= edit.offset && range.end < edit.offset + edit.length)
                removedSnapshot.lineEndings.push_back(buffer_.lineEnding(newline));
        }
        TextBuffer insertedBuffer(edit.insertedText);
        auto insertedSnapshot = insertedBuffer.snapshot();
        if (bareLineEnding != LineEnding::lf) {
            for (auto& ending : insertedSnapshot.lineEndings) {
                if (ending == LineEnding::lf) ending = bareLineEnding;
            }
        }
        buffer_.replace(edit.offset, edit.length, edit.insertedText, bareLineEnding);
        const auto insertedBytes = insertedSnapshot.utf8.size();
        cursors_.onTextReplaced(edit.offset, edit.length, insertedBytes);
        selections_.onTextReplaced(edit.offset, edit.length, insertedBytes);
        records.push_back({edit.offset, removedText, insertedSnapshot.utf8,
                           std::move(removedSnapshot), insertedSnapshot});
    }
    if (clearSelections) selections_.clearSelections();
    return records;
}

} // namespace note::editor
