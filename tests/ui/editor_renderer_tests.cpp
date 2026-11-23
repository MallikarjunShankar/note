#include "editor/cursor_manager.hpp"
#include "editor/selection_manager.hpp"
#include "editor/text_buffer.hpp"
#include "ui/editor_renderer.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QEventLoop>
#include <QFontMetricsF>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

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

bool imageContainsColor(const QImage& image, const QColor& color) {
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y) == color) return true;
    return false;
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

    require(renderer.configuration().tabWidth == 4 &&
                renderer.configuration().lineNumbers &&
                !renderer.configuration().wordWrap &&
                !renderer.configuration().matchingOccurrences,
            "presentation defaults are minimal and configurable");
    require(renderer.lineNumberGutterWidth() > 0 &&
                renderer.displayedLineNumber(0) == QStringLiteral("1"),
            "absolute line numbering reserves and displays its gutter");
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
    cursors.moveToLineColumn(50, 0);
    configuration.relativeLineNumbers = true;
    renderer.setConfiguration(configuration);
    require(renderer.displayedLineNumber(50) == QStringLiteral("51") &&
                renderer.displayedLineNumber(49) == QStringLiteral("1") &&
                renderer.displayedLineNumber(51) == QStringLiteral("1"),
            "relative numbering keeps the active line absolute");
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
    require(renderer.lastPaintedLineCount() > 0 &&
                renderer.lastPaintedLineCount() <=
                    renderer.visibleLineRange().end -
                        renderer.visibleLineRange().begin,
            "painting processes only visible lines and a bounded guard row");
    require(image.pixelColor(179, 49) == renderer.configuration().background,
            "painting fills configured background");
    renderer.setHorizontalOffset(12);
    require(renderer.horizontalOffset() == 12, "horizontal viewport positioning");
}

void testTextGeometryRespectsLineNumberGutter() {
    TextBuffer text("abcdef");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditorRenderer renderer(text, cursors, selections);
    renderer.resize(180, 60);
    auto configuration = renderer.configuration();
    configuration.matchingOccurrences = true;
    renderer.setConfiguration(configuration);
    renderer.show();
    QCoreApplication::processEvents();

    selections.setSelection(0, 0, text.sizeBytes());
    renderer.setMatchingBrackets(note::editor::BracketPair{0, 1});
    renderer.setHorizontalOffset(5);
    const auto gutter = renderer.lineNumberGutterWidth();
    const auto selection = renderer.selectionGeometry(0);
    const auto occurrences = renderer.matchingOccurrenceGeometry();
    const auto brackets = renderer.matchingBracketGeometry();
    require(!selection.isEmpty() && !occurrences.isEmpty() && !brackets.isEmpty(),
            "text geometry remains available after horizontal scrolling");
    for (const auto& geometry : selection)
        require(geometry.left() >= gutter,
                "selection geometry is clipped outside the line-number gutter");
    for (const auto& geometry : occurrences)
        require(geometry.left() >= gutter,
                "occurrence geometry is clipped outside the line-number gutter");
    for (const auto& geometry : brackets)
        require(geometry.left() >= gutter,
                "matching bracket geometry is clipped outside the gutter");
    require(renderer.cursorGeometry(0).isEmpty(),
            "a horizontally scrolled cursor cannot paint over line numbers");
}

void testPresentationDecorationsAndOccurrences() {
    TextBuffer text("foo foo foobar\n    bar\tbaz\nfoo");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditorRenderer renderer(text, cursors, selections);
    renderer.resize(360, 100);
    auto configuration = renderer.configuration();
    configuration.lineHeight = 20;
    configuration.indentationGuides = true;
    configuration.showWhitespace = true;
    configuration.matchingOccurrences = true;
    configuration.currentLine = QColor(QStringLiteral("#a13579"));
    configuration.matchingOccurrence = QColor(QStringLiteral("#2468ac"));
    configuration.indentationGuide = QColor(QStringLiteral("#12ab34"));
    configuration.whitespace = QColor(QStringLiteral("#de5412"));
    renderer.setConfiguration(configuration);
    renderer.show();
    QCoreApplication::processEvents();

    require(renderer.matchingOccurrenceGeometry().size() == 3,
            "the active identifier's exact visible occurrences are found");
    QImage image(renderer.viewport()->size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    renderer.viewport()->render(&image);
    require(imageContainsColor(image, configuration.currentLine) &&
                imageContainsColor(image, configuration.matchingOccurrence) &&
                imageContainsColor(image, configuration.indentationGuide) &&
                imageContainsColor(image, configuration.whitespace),
            "current line, occurrences, guides, and whitespace are painted");

    cursors.moveToLineColumn(1, 0);
    configuration.relativeLineNumbers = true;
    renderer.setConfiguration(configuration);
    require(renderer.displayedLineNumber(0) == QStringLiteral("1") &&
                renderer.displayedLineNumber(1) == QStringLiteral("2") &&
                renderer.displayedLineNumber(2) == QStringLiteral("1"),
            "relative line labels update when the primary cursor moves");
}

// Width as a fraction of a fixture line's rendered advance: wrap tests must not
// assume glyph widths (Qt 6.12's offscreen "FixedFont" is not even monospace).
int widthOf(const EditorRenderer& renderer, const char* line, qreal fraction) {
    return static_cast<int>(QFontMetricsF(renderer.configuration().font)
                                .horizontalAdvance(QString::fromUtf8(line)) * fraction);
}

void testWordWrappingAndWrappedGeometry() {
    TextBuffer text("abcdefghijabcdefghij\nnext");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditorRenderer renderer(text, cursors, selections);
    renderer.resize(widthOf(renderer, "abcdefghijabcdefghij", 0.625), 54);
    auto configuration = renderer.configuration();
    configuration.lineNumbers = false;
    configuration.lineHeight = 18;
    configuration.wordWrap = true;
    renderer.setConfiguration(configuration);
    renderer.show();
    QCoreApplication::processEvents();

    require(renderer.visualRowCount(0) >= 2 &&
                renderer.horizontalOffset() == 0,
            "word wrapping creates viewport-width visual rows");
    const auto visibleRows =
        static_cast<std::size_t>((renderer.viewport()->height() + 17) / 18);
    require(renderer.lastPaintedVisualRowCount() <= visibleRows + 1,
            "wrapped painting is bounded by the viewport and one guard row");
    const auto secondRowOffset = renderer.offsetAtPosition(QPointF(0, 19));
    require(secondRowOffset > 0 && text.isScalarBoundary(secondRowOffset),
            "hit testing maps wrapped rows to scalar-safe byte offsets");
    const QPointF localPosition(1, 1);
    const QPointF globalPosition(renderer.mapToGlobal(localPosition.toPoint()));
    QWheelEvent scrollDown(localPosition, globalPosition, QPoint(), QPoint(0, -120),
                           Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
    QApplication::sendEvent(renderer.viewport(), &scrollDown);
    require(renderer.firstVisibleSubrow() == 1,
            "wheel scrolling advances through a wrapped subrow");
    require(renderer.offsetAtPosition(QPointF(0, 1)) == secondRowOffset,
            "hit testing follows the wrapped subrow after scrolling");
    renderer.scrollToLine(0);

    selections.setSelection(0, 0, 20);
    require(renderer.selectionGeometry(0).size() >= 2,
            "selection geometry follows wrapped visual rows");
    cursors.setPrimaryCursor(20);
    renderer.ensureCursorVisible(20);
    const auto caret = renderer.cursorGeometry(0);
    require(caret.height() > 0 && caret.top() >= 0 &&
                caret.bottom() <= renderer.viewport()->height(),
            "cursor visibility accounts for wrapped row positions");

    TextBuffer unicode("λ界 λ界 λ界 λ界");
    CursorManager unicodeCursors(unicode);
    SelectionManager unicodeSelections(unicode);
    EditorRenderer unicodeRenderer(unicode, unicodeCursors, unicodeSelections);
    unicodeRenderer.resize(72, 36);
    auto unicodeConfiguration = unicodeRenderer.configuration();
    unicodeConfiguration.lineNumbers = false;
    unicodeConfiguration.wordWrap = true;
    unicodeConfiguration.lineHeight = 18;
    unicodeRenderer.setConfiguration(unicodeConfiguration);
    unicodeRenderer.show();
    QCoreApplication::processEvents();
    require(unicodeRenderer.visualRowCount(0) > 1 &&
                unicodeRenderer.offsetAtPosition(QPointF(0, 19)) > 0 &&
                unicode.isScalarBoundary(
                    unicodeRenderer.offsetAtPosition(QPointF(0, 19))),
            "Unicode wrapped hit testing preserves UTF-8 scalar boundaries");
}

void testWrappedScrollbarUsesVisualRows() {
    TextBuffer text("abcdefghijabcdefghij\nshort\n"
                    "klmnopqrstklmnopqrst\nanother\n"
                    "uvwxyzuvwxyzuvwxyz\nlast");
    CursorManager cursors(text);
    SelectionManager selections(text);
    EditorRenderer renderer(text, cursors, selections);
    renderer.resize(widthOf(renderer, "abcdefghijabcdefghij", 0.65), 72);
    auto configuration = renderer.configuration();
    configuration.lineNumbers = false;
    configuration.lineHeight = 18;
    configuration.wordWrap = true;
    renderer.setConfiguration(configuration);
    renderer.show();

    QEventLoop wait;
    QTimer poll;
    poll.setInterval(2);
    QObject::connect(&poll, &QTimer::timeout, &wait, [&] {
        if (renderer.wrappedRowIndexComplete()) wait.quit();
    });
    QTimer::singleShot(3000, &wait, &QEventLoop::quit);
    poll.start();
    wait.exec();
    require(renderer.wrappedRowIndexComplete(),
            "wrapped row index completes incrementally");

    std::size_t expectedRows = 0;
    for (std::size_t line = 0; line < text.lineCount(); ++line)
        expectedRows += renderer.visualRowCount(line);
    require(renderer.totalVisualRowCount() == expectedRows,
            "wrapped row index counts all visual rows");
    const auto viewportRows = static_cast<std::size_t>(
        (renderer.viewport()->height() + configuration.lineHeight - 1) /
        configuration.lineHeight);
    const auto expectedMaximum = expectedRows > viewportRows
        ? expectedRows - viewportRows : 0;
    require(static_cast<std::size_t>(
                renderer.verticalScrollBar()->maximum()) == expectedMaximum &&
                renderer.verticalScrollBar()->pageStep() ==
                    static_cast<int>(viewportRows),
            "wrapped scrollbar range and thumb page size use visual rows");
    renderer.scrollToLine(2);
    require(renderer.firstVisibleLine() == 2,
            "scrollbar visual-row mapping retains logical line navigation");
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        testConfigurationAndViewport();
        testCursorAndSelectionGeometry();
        testMatchingBracketGeometry();
        testPaintingAndClipping();
        testTextGeometryRespectsLineNumberGutter();
        testPresentationDecorationsAndOccurrences();
        testWordWrappingAndWrappedGeometry();
        testWrappedScrollbarUsesVisualRows();
        std::cout << "All editor renderer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Editor renderer test failure: " << error.what() << '\n';
        return 1;
    }
}
