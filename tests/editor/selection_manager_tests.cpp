#include "editor/selection_manager.hpp"
#include "editor/text_buffer.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using note::editor::SelectionManager;
using note::editor::TextBuffer;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testDirectionsAndClearing() {
    TextBuffer text("abcd");
    SelectionManager selections(text);
    require(selections.selectionCount() == 1 && !selections.hasSelection(0),
            "manager starts with one empty selection");
    selections.setSelection(0, 3, 1);
    require(selections.hasSelection(0), "backward selection is detected");
    require(selections.normalizedRange(0).begin == 1 &&
                selections.normalizedRange(0).end == 3,
            "backward selection is normalized");
    selections.clearSelection(0);
    require(!selections.hasSelection(0), "selection can be cleared");
    selections.addSelection(0, 2);
    selections.addSelection(4, 4);
    require(selections.selectionCount() == 3 && selections.hasSelection(1) &&
                !selections.hasSelection(2),
            "multiple independent selections are retained");
    selections.clearSelections();
    require(selections.selectionCount() == 0, "all selections can be cleared");
}

void testUtf8AndBoundaries() {
    TextBuffer text("aλ🙂z");
    SelectionManager selections(text);
    selections.setSelection(0, 2, 5);
    require(selections.selection(0).anchor == 1 &&
                selections.selection(0).active == 3,
            "endpoints clamp to preceding UTF-8 scalar boundaries");
    selections.setActive(0, 100);
    require(selections.selection(0).active == text.sizeBytes(),
            "endpoint clamps to document end");
    selections.setAnchor(0, 100);
    require(selections.selection(0).anchor == text.sizeBytes(),
            "invalid endpoint is safely clamped");
}

void testEditsAndInvalidTransforms() {
    TextBuffer text("abλcd");
    SelectionManager selections(text);
    selections.setSelection(0, 1, 5);
    selections.addSelection(0, 1);

    text.insert(1, "XX");
    selections.onTextReplaced(1, 0, 2);
    require(selections.selection(0).anchor == 3 && selections.selection(0).active == 7,
            "insertion shifts both endpoints");
    require(selections.selection(1).anchor == 0 && selections.selection(1).active == 3,
            "insertion preserves endpoints before the edit");

    text.erase(4, 2);
    selections.onTextReplaced(4, 2, 0);
    require(selections.selection(0).anchor == 3 && selections.selection(0).active == 5,
            "deletion maps endpoints after the removed range");
    selections.syncToBuffer();
    require(selections.selection(0).active <= text.sizeBytes(),
            "post-edit endpoints remain in document bounds");

    bool rejected = false;
    try {
        selections.onTextReplaced(100, 0, 1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "invalid edit transformation is rejected");
}

void testEmptyDocument() {
    TextBuffer text;
    SelectionManager selections(text);
    selections.setSelection(0, 100, 200);
    require(selections.selection(0).anchor == 0 && selections.selection(0).active == 0,
            "empty document endpoints remain at zero");
    selections.onTextReplaced(0, 0, 0);
    require(!selections.hasSelection(0), "empty selection remains empty");
}

} // namespace

int main() {
    try {
        testDirectionsAndClearing();
        testUtf8AndBoundaries();
        testEditsAndInvalidTransforms();
        testEmptyDocument();
        std::cout << "All selection manager tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Selection manager test failure: " << error.what() << '\n';
        return 1;
    }
}
