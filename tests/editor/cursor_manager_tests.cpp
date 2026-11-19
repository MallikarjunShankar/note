#include "editor/cursor_manager.hpp"
#include "editor/text_buffer.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using note::editor::CursorManager;
using note::editor::TextBuffer;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testEmptyAndBoundaries() {
    TextBuffer empty;
    CursorManager cursors(empty);
    require(cursors.primaryOffset() == 0, "empty document starts at zero");
    require(cursors.lineCount() == 1, "empty document has one line");
    require(cursors.lineAndColumn(0).line == 0 && cursors.lineAndColumn(0).column == 0,
            "empty document line and column");

    cursors.setPrimaryCursor(100);
    require(cursors.primaryOffset() == 0, "empty document position is clamped");
    cursors.moveRight();
    cursors.moveLeft();
    require(cursors.primaryOffset() == 0, "empty document horizontal movement is stable");
    cursors.moveUp();
    cursors.moveDown();
    require(cursors.primaryOffset() == 0, "empty document vertical movement is stable");
}

void testUtf8AndClamping() {
    TextBuffer text("aλ🙂z");
    CursorManager cursors(text);
    require(cursors.primaryOffset() == 0, "primary cursor starts at document start");

    cursors.setPrimaryCursor(text.sizeBytes());
    require(cursors.primaryOffset() == text.sizeBytes(), "end position accepted");
    cursors.moveLeft();
    require(cursors.primaryOffset() == 7, "left moves to scalar start before final ASCII z");
    cursors.moveLeft();
    require(cursors.primaryOffset() == 3, "left moves across four-byte emoji");

    cursors.setPrimaryCursor(2);
    require(cursors.primaryOffset() == 1, "position inside scalar clamps to preceding scalar boundary");
    cursors.setPrimaryCursor(5);
    require(cursors.primaryOffset() == 3, "position inside scalar after lambda clamps safely");

    cursors.setPrimaryCursor(100);
    require(cursors.primaryOffset() == text.sizeBytes(), "oversized position clamps to document end");

    cursors.moveToLineColumn(0, 2);
    require(cursors.primaryOffset() == 1, "line/column byte offset clamps to scalar boundary");
    require(cursors.lineAndColumn(cursors.primaryOffset()).line == 0 &&
            cursors.lineAndColumn(cursors.primaryOffset()).column == 1,
            "line/column reports UTF-8 byte column");
}

void testLinesAndVerticalMovement() {
    TextBuffer text("ab\nλ\n🙂");
    CursorManager cursors(text);
    cursors.moveToLineColumn(0, 2);
    require(cursors.primaryOffset() == 2, "end of first line");
    cursors.moveDown();
    require(cursors.primaryOffset() == 5, "down preserves target column on lambda");
    cursors.moveDown();
    require(cursors.primaryOffset() == 6, "down on lambda line clamps to emoji line");
    cursors.moveDown();
    require(cursors.primaryOffset() == 6, "down on final line remains at final line");
    cursors.moveUp();
    require(cursors.primaryOffset() == 5, "up restores preserved lambda column");

    cursors.moveToLineColumn(2, 100);
    require(cursors.primaryOffset() == text.sizeBytes(), "line/column column clamps to line end");
    cursors.moveToDocumentStart();
    require(cursors.primaryOffset() == 0, "document start navigation");
    cursors.moveToDocumentEnd();
    require(cursors.primaryOffset() == text.sizeBytes(), "document end navigation");
}

void testTrailingAndEmptyLines() {
    TextBuffer text("a\n\n");
    CursorManager cursors(text);
    require(cursors.lineCount() == 3, "trailing newline has final empty line");
    cursors.moveToLineColumn(1, 0);
    require(cursors.primaryOffset() == 2, "middle empty line position");
    cursors.moveDown();
    require(cursors.primaryOffset() == 3, "down reaches final empty line");
    cursors.moveToLineColumn(0, 1);
    cursors.moveRight();
    require(cursors.primaryOffset() == 2, "right at EOL lands on next line start");
    cursors.moveLeft();
    require(cursors.primaryOffset() == 1, "left from next line start lands at preceding EOL");

    TextBuffer two("\n");
    CursorManager firstLine(two);
    require(firstLine.lineAndColumn(0).line == 0, "newline offset belongs to empty first line");
    require(firstLine.lineAndColumn(1).line == 1, "offset after newline belongs to final empty line");
}

void testSecondaryCursors() {
    TextBuffer text("alpha\nbeta");
    CursorManager cursors(text);
    require(cursors.addSecondaryCursor(2), "add secondary cursor");
    require(cursors.addSecondaryCursor(8), "add another secondary cursor");
    require(!cursors.addSecondaryCursor(2), "duplicate secondary cursor is rejected");
    require(cursors.cursorCount() == 3, "primary plus two secondary cursors");
    require(cursors.secondaryCursorCount() == 2, "secondary cursor count");
    require(cursors.secondaryCursorOffsets() == std::vector<std::size_t>({2, 8}),
            "secondary cursor offsets");

    cursors.moveRight();
    require(cursors.primaryOffset() == 1, "primary moves right");
    require(cursors.secondaryCursorOffset(0) == 3, "first secondary moves right");
    require(cursors.secondaryCursorOffset(1) == 9, "second secondary moves right");

    cursors.removeSecondaryCursor(0);
    require(cursors.secondaryCursorCount() == 1, "remove secondary cursor");
    require(cursors.secondaryCursorOffset(0) == 9, "remaining secondary preserved");
    cursors.clearSecondaryCursors();
    require(cursors.secondaryCursorCount() == 0, "clear secondary cursors");

    cursors.setPrimaryCursor(100);
    require(cursors.primaryOffset() == text.sizeBytes(), "secondary-free primary still clamps");
}

void testPostEditAdjustment() {
    TextBuffer text("abλcd");
    CursorManager cursors(text);
    cursors.setPrimaryCursor(5);
    cursors.addSecondaryCursor(1);

    text.insert(1, "xx");
    cursors.onTextReplaced(1, 0, 2);
    require(cursors.primaryOffset() == 7, "insertion shifts cursors after insertion");
    require(cursors.secondaryCursorOffset(0) == 3, "insertion shifts secondary cursor");

    text.erase(4, 2);
    cursors.onTextReplaced(4, 2, 0);
    require(cursors.primaryOffset() == 5, "deletion shifts cursor after removed range");
    require(cursors.secondaryCursorOffset(0) == 3, "cursor inside deletion maps to deletion offset");

    cursors.syncToBuffer();
    require(cursors.primaryOffset() == 5, "sync leaves valid position stable");
}

} // namespace

int main() {
    try {
        testEmptyAndBoundaries();
        testUtf8AndClamping();
        testLinesAndVerticalMovement();
        testTrailingAndEmptyLines();
        testSecondaryCursors();
        testPostEditAdjustment();
        std::cout << "All cursor manager tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Cursor manager test failure: " << error.what() << '\n';
        return 1;
    }
}
