#include "editor/cursor_manager.hpp"
#include "editor/editing_engine.hpp"
#include "editor/selection_manager.hpp"
#include "editor/text_buffer.hpp"
#include "editor/undo_redo_manager.hpp"

#include <iostream>
#include <stdexcept>

using note::editor::CursorManager;
using note::editor::EditingEngine;
using note::editor::LineEnding;
using note::editor::SelectionManager;
using note::editor::TextBuffer;
using note::editor::UndoRedoManager;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testInsertDeleteAndEmptyHistory() {
    TextBuffer text;
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    UndoRedoManager history(text, cursors, selections, editing);

    require(!history.undo() && !history.redo(), "empty history is safe");
    (void)history.insertText("abc");
    require(text.text() == "abc" && history.undoCount() == 1,
            "insertion is recorded");
    require(history.undo(), "insertion undo");
    require(text.text().empty() && cursors.primaryOffset() == 0,
            "insertion undo restores document and cursor");
    require(history.redo() && text.text() == "abc" &&
                cursors.primaryOffset() == 3,
            "insertion redo");

    (void)history.backspace();
    require(text.text() == "ab", "backspace is recorded");
    require(history.undo() && text.text() == "abc", "backspace undo");
    cursors.setPrimaryCursor(0);
    (void)history.deleteForward();
    require(text.text() == "bc", "forward deletion is recorded");
    require(history.undo() && text.text() == "abc", "forward deletion undo");
}

void testGroupingAndBoundaries() {
    TextBuffer text;
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    UndoRedoManager history(text, cursors, selections, editing);

    (void)history.insertText("a");
    (void)history.insertText("b");
    (void)history.insertText("c");
    require(history.undoCount() == 1, "typing is grouped");
    require(history.undo() && text.text().empty(), "grouped typing undo");
    require(history.redo() && text.text() == "abc", "grouped typing redo");

    (void)history.backspace();
    (void)history.backspace();
    require(history.undoCount() == 2, "backspace group follows typing group");
    require(history.undo() && text.text() == "abc", "grouped backspace undo");

    history.beginTransaction();
    (void)history.insertText("x");
    (void)history.insertText("y");
    history.endTransaction();
    require(history.undoCount() == 2, "explicit transaction is one entry");
    require(history.undo() && text.text() == "abc", "explicit transaction undo");
}

void testSelectionCursorAndMultiCursorState() {
    TextBuffer text("hello world");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    UndoRedoManager history(text, cursors, selections, editing);

    cursors.setPrimaryCursor(0);
    require(cursors.addSecondaryCursor(6), "secondary cursor");
    selections.setSelection(0, 0, 5);
    selections.addSelection(6, 11);
    (void)history.replaceSelections("X");
    require(text.text() == "X X" && selections.selectionCount() == 0,
            "multi-selection replacement");
    require(history.undo() && text.text() == "hello world" &&
                cursors.primaryOffset() == 0 &&
                cursors.secondaryCursorOffset(0) == 6 &&
                selections.selectionCount() == 2,
            "undo restores multi-cursor and selection state");
    require(history.redo(), "multi-selection redo succeeds");
    require(text.text() == "X X", "redo restores replaced text");
    require(cursors.primaryOffset() == 1 &&
                cursors.secondaryCursorOffset(0) == 3,
            "redo restores post-edit cursors");
    require(selections.selectionCount() == 0,
            "redo restores cleared selections");
}

void testUnicodeAndLineEndings() {
    TextBuffer text("a\r\nλ\rb\nc");
    const auto original = text.snapshot();
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    UndoRedoManager history(text, cursors, selections, editing);

    selections.setSelection(0, 0, 4);
    (void)history.replaceSelections("🙂\r\n");
    require(history.undo(), "unicode replacement undo");
    const auto restored = text.snapshot();
    require(restored.utf8 == original.utf8 && restored.lineEndings == original.lineEndings,
            "undo restores exact line-ending metadata");
    require(history.redo() && text.lineEnding(0) == LineEnding::crlf,
            "redo restores inserted line ending");

    TextBuffer newlineText("a\n");
    CursorManager newlineCursors(newlineText);
    SelectionManager newlineSelections(newlineText);
    EditingEngine newlineEditing(newlineText, newlineCursors, newlineSelections);
    UndoRedoManager newlineHistory(newlineText, newlineCursors,
                                   newlineSelections, newlineEditing);
    newlineCursors.setPrimaryCursor(newlineText.sizeBytes());
    (void)newlineHistory.insertNewline(LineEnding::crlf);
    require(newlineHistory.undo() && newlineText.lineEnding(0) == LineEnding::lf,
            "newline undo restores original metadata");
    require(newlineHistory.redo() && newlineText.lineEnding(1) == LineEnding::crlf,
            "newline redo restores requested metadata");
}

void testRedoInvalidationAndMemoryLimit() {
    TextBuffer text;
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    UndoRedoManager history(text, cursors, selections, editing);

    (void)history.insertText("a");
    require(history.undo(), "prepare redo history");
    require(history.redoCount() == 1, "redo exists after undo");
    (void)history.insertText("b");
    require(history.redoCount() == 0 && !history.redo(),
            "new edit clears redo history");

    history.clearHistory();
    history.setMemoryLimit(1);
    (void)history.insertText("large");
    require(history.undoCount() == 0 && history.memoryUsage() <= 1,
            "oversized transaction is safely omitted");
}

void testFailureSafety() {
    TextBuffer text("abcdef");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    UndoRedoManager history(text, cursors, selections, editing);
    selections.setSelection(0, 0, 3);
    selections.addSelection(2, 5);
    bool rejected = false;
    try {
        (void)history.deleteSelections();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && text.text() == "abcdef" && history.undoCount() == 0,
            "failed transaction does not partially mutate or record");
}

} // namespace

int main() {
    try {
        testInsertDeleteAndEmptyHistory();
        testGroupingAndBoundaries();
        testSelectionCursorAndMultiCursorState();
        testUnicodeAndLineEndings();
        testRedoInvalidationAndMemoryLimit();
        testFailureSafety();
        std::cout << "All undo/redo manager tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Undo/redo manager test failure: " << error.what() << '\n';
        return 1;
    }
}
