#include "editor/cursor_manager.hpp"
#include "editor/editing_engine.hpp"
#include "editor/selection_manager.hpp"
#include "editor/text_buffer.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

using note::editor::CursorManager;
using note::editor::EditingEngine;
using note::editor::SelectionManager;
using note::editor::TextBuffer;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testBasicEditing() {
    TextBuffer text;
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);

    (void)editing.insertText("abc");
    require(text.text() == "abc" && cursors.primaryOffset() == 3,
            "insert at empty document");
    (void)editing.backspace();
    require(text.text() == "ab" && cursors.primaryOffset() == 2,
            "backspace at document end");
    cursors.setPrimaryCursor(0);
    (void)editing.deleteForward();
    require(text.text() == "b" && cursors.primaryOffset() == 0,
            "delete at document start");
    (void)editing.insertNewline();
    require(text.text() == "\nb", "newline insertion");
}

void testSelectionsAndUnicode() {
    TextBuffer text("aλ🙂z");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);

    selections.setSelection(0, 1, 7);
    const auto records = editing.replaceSelections("X");
    require(records.size() == 1 && records[0].removedText == "λ🙂" &&
                text.text() == "aXz" && cursors.primaryOffset() == 2,
            "replace multibyte selection");
    require(selections.selectionCount() == 0, "replacement clears selections");

    cursors.setPrimaryCursor(2);
    (void)editing.backspace();
    require(text.text() == "az" && cursors.primaryOffset() == 1,
            "backspace respects scalar boundaries");
    cursors.setPrimaryCursor(text.sizeBytes());
    (void)editing.deleteForward();
    require(text.text() == "az", "delete at document end is a no-op");
}

void testMultipleCursorsAndDeterminism() {
    TextBuffer text("abcd");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    cursors.setPrimaryCursor(1);
    require(cursors.addSecondaryCursor(3), "add second cursor");
    (void)editing.insertText("X");
    require(text.text() == "aXbcXd", "multiple inserts apply safely");
    require(cursors.primaryOffset() == 2 && cursors.secondaryCursorOffset(0) == 5,
            "multiple cursor offsets update");

    cursors.clearSecondaryCursors();
    cursors.setPrimaryCursor(1);
    require(cursors.addSecondaryCursor(2), "add adjacent cursor");
    selections.clearSelections();
    selections.addSelection(1, 1);
    selections.addSelection(1, 1);
    bool rejected = false;
    try {
        (void)editing.replaceSelections("Y");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "duplicate edit offsets are rejected");
}

void testMixedSelectionAndCursorDeletion() {
    TextBuffer backwardText("abcd");
    CursorManager backwardCursors(backwardText);
    SelectionManager backwardSelections(backwardText);
    EditingEngine backwardEditing(backwardText, backwardCursors,
                                  backwardSelections);
    backwardCursors.setPrimaryCursor(2);
    require(backwardCursors.addSecondaryCursor(4), "add backspace cursor");
    backwardSelections.setSelection(0, 1, 2);
    backwardSelections.addSelection(4, 4);
    const auto backward = backwardEditing.backspace();
    require(backward.size() == 2 && backwardText.text() == "ac" &&
                backwardSelections.selectionCount() == 0,
            "backspace deletes selections and adjacent text at empty cursors atomically");

    TextBuffer forwardText("abcd");
    CursorManager forwardCursors(forwardText);
    SelectionManager forwardSelections(forwardText);
    EditingEngine forwardEditing(forwardText, forwardCursors, forwardSelections);
    forwardCursors.setPrimaryCursor(1);
    require(forwardCursors.addSecondaryCursor(3), "add forward-delete cursor");
    forwardSelections.setSelection(0, 1, 2);
    forwardSelections.addSelection(3, 3);
    const auto forward = forwardEditing.deleteForward();
    require(forward.size() == 2 && forwardText.text() == "ac" &&
                forwardSelections.selectionCount() == 0,
            "forward delete combines selected and unselected cursor ranges");
}

void testInsertAtSelectedCursorIndices() {
    TextBuffer text("a)b");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    cursors.setPrimaryCursor(1);
    require(cursors.addSecondaryCursor(3), "add targeted insertion cursor");
    const std::array<std::size_t, 1> targets{1};
    const auto records = editing.insertTextAtCursorIndices(targets, ")");
    require(records.size() == 1 && text.text() == "a)b)",
            "targeted insertion leaves unselected cursor positions untouched");
    require(cursors.primaryOffset() == 1 && cursors.secondaryCursorOffset(0) == 4,
            "targeted insertion transforms all cursor positions");

    bool rejected = false;
    const std::array<std::size_t, 2> invalidTargets{0, 0};
    try {
        (void)editing.insertTextAtCursorIndices(invalidTargets, "x");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && text.text() == "a)b)",
            "duplicate targeted cursor indices fail before mutation");
}

void testOverlappingSelectionsAndLineEndings() {
    TextBuffer text("one\ntwo");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditingEngine editing(text, cursors, selections);
    selections.setSelection(0, 0, 4);
    selections.addSelection(2, 6);
    bool rejected = false;
    try {
        (void)editing.deleteSelections();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && text.text() == "one\ntwo",
            "overlapping selections are rejected atomically");

    selections.clearSelections();
    selections.addSelection(3, 3);
    cursors.setPrimaryCursor(3);
    cursors.clearSecondaryCursors();
    (void)editing.insertNewline(note::editor::LineEnding::crlf);
    require(text.text() == "one\n\ntwo" &&
                text.lineEnding(0) == note::editor::LineEnding::crlf &&
                text.lineEnding(1) == note::editor::LineEnding::lf,
            "newline preserves line-ending metadata");
}

} // namespace

int main() {
    try {
        testBasicEditing();
        testSelectionsAndUnicode();
        testMultipleCursorsAndDeterminism();
        testMixedSelectionAndCursorDeletion();
        testInsertAtSelectedCursorIndices();
        testOverlappingSelectionsAndLineEndings();
        std::cout << "All editing engine tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Editing engine test failure: " << error.what() << '\n';
        return 1;
    }
}
