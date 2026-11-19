#include "editor/cursor_manager.hpp"
#include "editor/selection_manager.hpp"
#include "editor/text_buffer.hpp"
#include "ui/editor_renderer.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QImage>

#include <iostream>
#include <stdexcept>

using note::editor::CursorManager;
using note::editor::SelectionManager;
using note::editor::TextBuffer;
using note::ui::EditorRenderConfiguration;
using note::ui::EditorRenderer;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testConfigurationAndViewport() {
    TextBuffer text;
    for (int line = 0; line < 200; ++line) text.insert(text.sizeBytes(), "line\n");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditorRenderer renderer(text, cursors, selections);
    renderer.resize(320, 100);
    renderer.show();
    QCoreApplication::processEvents();

    require(renderer.configuration().tabWidth == 4, "default tab width");
    require(renderer.visibleLineRange().begin == 0 &&
                renderer.visibleLineRange().end < text.lineCount(),
            "viewport only exposes visible lines");
    renderer.scrollToLine(100);
    require(renderer.firstVisibleLine() == 100, "vertical scrolling");

    auto configuration = renderer.configuration();
    configuration.tabWidth = 8;
    configuration.lineHeight = 24;
    renderer.setConfiguration(configuration);
    require(renderer.configuration().tabWidth == 8 &&
                renderer.configuration().lineHeight == 24,
            "render configuration updates");
}

void testCursorAndSelectionGeometry() {
    TextBuffer text("a\tλ\nsecond line\nthird");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditorRenderer renderer(text, cursors, selections);
    renderer.resize(400, 200);
    renderer.show();
    QCoreApplication::processEvents();

    cursors.setPrimaryCursor(5);
    const auto cursor = renderer.cursorGeometry(0);
    require(cursor.height() > 0 && cursor.y() > 0,
            "cursor geometry accounts for line position");

    selections.setSelection(0, 0, 10);
    const auto rectangles = renderer.selectionGeometry(0);
    require(rectangles.size() >= 2, "multiline selection geometry");
    require(rectangles.front().width() > 0 && rectangles.back().width() > 0,
            "selection rectangles have visible widths");

    cursors.setPrimaryCursor(0);
    const auto startCursor = renderer.cursorGeometry(0);
    cursors.setPrimaryCursor(2);
    const auto tabbedCursor = renderer.cursorGeometry(0);
    require(tabbedCursor.x() > startCursor.x(), "tab and Unicode layout advances");

    require(cursors.addSecondaryCursor(1), "secondary cursor is created");
    const auto secondaryCursor = renderer.cursorGeometry(1);
    require(secondaryCursor.width() > 0 && secondaryCursor.x() > startCursor.x(),
            "renderer exposes geometry for secondary cursors");
}

void testMatchingBracketGeometry() {
    TextBuffer text("()");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditorRenderer renderer(text, cursors, selections);
    renderer.resize(180, 60);
    renderer.show();
    QCoreApplication::processEvents();

    renderer.setMatchingBrackets(note::editor::BracketPair{0, 1});
    const auto geometry = renderer.matchingBracketGeometry();
    require(geometry.size() == 2 && geometry[0].width() > 0 &&
                geometry[1].width() > 0,
            "both visible matching brackets receive highlight geometry");
    QImage image(renderer.viewport()->size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    renderer.viewport()->render(&image);
    const auto sampleX = qRound(geometry.front().left() +
                                geometry.front().width() / 2);
    const auto sampleY = qRound(geometry.front().top());
    require(image.pixelColor(sampleX, sampleY) ==
                renderer.configuration().matchingBracket,
            "matching bracket feedback is painted with its configured color");
}

void testPaintingAndClipping() {
    TextBuffer text("one\ntwo\nthree\nfour\nfive");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditorRenderer renderer(text, cursors, selections);
    renderer.resize(180, 50);
    renderer.show();
    QImage image(renderer.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    renderer.render(&image);
    require(renderer.lastPaintedLineCount() ==
                renderer.visibleLineRange().end - renderer.visibleLineRange().begin,
            "painting processes only visible lines");
    require(image.pixelColor(179, 49) == renderer.configuration().background,
            "painting fills configured background");
    renderer.setHorizontalOffset(12);
    require(renderer.horizontalOffset() == 12, "horizontal viewport positioning");
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        testConfigurationAndViewport();
        testCursorAndSelectionGeometry();
        testMatchingBracketGeometry();
        testPaintingAndClipping();
        std::cout << "All editor renderer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Editor renderer test failure: " << error.what() << '\n';
        return 1;
    }
}
