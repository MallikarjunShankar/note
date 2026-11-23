#include "ui/editor_renderer.hpp"

#include "editor/cursor_manager.hpp"
#include "editor/text_buffer.hpp"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace note::ui {

EditorRenderer::EditorRenderer(const editor::TextBuffer& buffer,
                               const editor::CursorManager& cursors,
                               const editor::SelectionManager& selections,
                               QWidget* parent)
    : QAbstractScrollArea(parent), buffer_(buffer), cursors_(cursors),
      selections_(selections) {
    configuration_.font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    verticalScrollBar()->setSingleStep(1);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this] {
                firstVisibleSubrow_ = 0;
                viewport()->update();
            });
    updateScrollBar();
}

const EditorRenderConfiguration& EditorRenderer::configuration() const noexcept {
    return configuration_;
}

void EditorRenderer::setConfiguration(EditorRenderConfiguration configuration) {
    if (configuration.tabWidth < 1) configuration.tabWidth = 1;
    if (configuration.font.family().isEmpty())
        configuration.font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    configuration_ = std::move(configuration);
    firstVisibleSubrow_ = 0;
    if (configuration_.wordWrap) {
        horizontalOffset_ = 0;
    }
    updateScrollBar();
    invalidateViewport();
}

VisibleLineRange EditorRenderer::visibleLineRange() const noexcept {
    return calculateVisibleLineRange();
}

std::size_t EditorRenderer::firstVisibleLine() const noexcept {
    return static_cast<std::size_t>(verticalScrollBar()->value());
}

std::size_t EditorRenderer::firstVisibleSubrow() const noexcept {
    return firstVisibleSubrow_;
}

qreal EditorRenderer::lineNumberGutterWidth() const {
    if (!configuration_.lineNumbers) return 0;
    const auto digits =
        QString::number(static_cast<qulonglong>(buffer_.lineCount())).size();
    const QFontMetricsF metrics(configuration_.font);
    return metrics.horizontalAdvance(QString(digits, QLatin1Char('8'))) +
           2 * metrics.horizontalAdvance(QLatin1Char(' '));
}

qreal EditorRenderer::textOriginX() const {
    return lineNumberGutterWidth();
}

QString EditorRenderer::displayedLineNumber(std::size_t line) const {
    if (line >= buffer_.lineCount()) return {};
    const auto activeLine = cursors_.lineAndColumn(cursors_.primaryOffset()).line;
    if (configuration_.relativeLineNumbers && line != activeLine)
        return QString::number(static_cast<qulonglong>(
            line > activeLine ? line - activeLine : activeLine - line));
    return QString::number(static_cast<qulonglong>(line + 1));
}

std::size_t EditorRenderer::visualRowCount(std::size_t line) const {
    if (line >= buffer_.lineCount()) return 0;
    return static_cast<std::size_t>(layoutLine(line).rows.size());
}

void EditorRenderer::scrollToLine(std::size_t line) {
    const auto maximum = verticalScrollBar()->maximum();
    firstVisibleSubrow_ = 0;
    verticalScrollBar()->setValue(static_cast<int>(
        std::min<std::size_t>(line, static_cast<std::size_t>(maximum))));
    viewport()->update();
}

void EditorRenderer::setHorizontalOffset(qreal offset) {
    horizontalOffset_ = configuration_.wordWrap ? 0 : std::max<qreal>(0, offset);
    invalidateViewport();
}

qreal EditorRenderer::horizontalOffset() const noexcept {
    return horizontalOffset_;
}

void EditorRenderer::setMatchingBrackets(
    std::optional<editor::BracketPair> pair) {
    if (matchingBrackets_ == pair) return;
    matchingBrackets_ = pair;
    invalidateViewport();
}

void EditorRenderer::setSearchMatches(
    const QVector<QPair<std::size_t, std::size_t>>& matches) {
    searchMatches_ = matches;
    invalidateViewport();
}

std::size_t EditorRenderer::offsetAtPosition(const QPointF& position) const {
    const auto lineCount = buffer_.lineCount();
    auto targetRow = static_cast<std::size_t>(
        std::max<qreal>(0, position.y()) / effectiveLineHeight());
    targetRow += firstVisibleSubrow_;
    auto line = firstVisibleLine();
    auto layout = layoutLine(line);
    while (line + 1 < lineCount && targetRow >=
               static_cast<std::size_t>(layout.rows.size())) {
        targetRow -= static_cast<std::size_t>(layout.rows.size());
        layout = layoutLine(++line);
    }
    const auto rowIndex = std::min(
        targetRow, static_cast<std::size_t>(layout.rows.size() - 1));
    const auto range = buffer_.lineRange(line);
    const auto& row = layout.rows[static_cast<int>(rowIndex)];
    const auto targetX = std::max<qreal>(
        0, position.x() - lineNumberGutterWidth() + horizontalOffset_);
    std::size_t best = range.begin + row.byteBegin;
    auto bestDistance = std::abs(targetX);
    for (auto offset = best + 1; offset <= range.begin + row.byteEnd; ++offset) {
        if (!buffer_.isScalarBoundary(offset)) continue;
        const auto x = xForByte(layout, range.begin, offset, rowIndex);
        const auto distance = std::abs(targetX - x);
        if (distance < bestDistance) {
            best = offset;
            bestDistance = distance;
        }
    }
    return best;
}

void EditorRenderer::ensureCursorVisible(std::size_t offset) {
    const auto location = cursors_.lineAndColumn(offset);
    const auto lineLayout = layoutLine(location.line);
    const auto cursorRow = rowForByte(lineLayout,
                                      buffer_.lineRange(location.line).begin,
                                      offset);
    const auto viewportRows = static_cast<std::size_t>(
        std::max(1, viewport()->height() / effectiveLineHeight()));
    const auto first = firstVisibleLine();
    if (!configuration_.wordWrap) {
        if (location.line < first) {
            scrollToLine(location.line);
        } else if (location.line >= first + viewportRows) {
            scrollToLine(location.line - viewportRows + 1);
        }
    } else {
        auto screenRow = -static_cast<std::ptrdiff_t>(firstVisibleSubrow_);
        if (location.line < first) {
            scrollToLine(location.line);
            screenRow = static_cast<std::ptrdiff_t>(cursorRow);
        } else if (location.line == first) {
            screenRow += static_cast<std::ptrdiff_t>(cursorRow);
        } else {
            const auto visible = calculateVisibleLineRange();
            if (location.line >= visible.end) {
                scrollToLine(location.line);
                screenRow = static_cast<std::ptrdiff_t>(cursorRow);
            } else {
                for (auto line = first; line < location.line; ++line)
                    screenRow += static_cast<std::ptrdiff_t>(
                        visualRowCount(line));
                screenRow += static_cast<std::ptrdiff_t>(cursorRow);
            }
        }

        if (screenRow < 0) {
            firstVisibleSubrow_ = cursorRow;
            viewport()->update();
        } else if (screenRow >= static_cast<std::ptrdiff_t>(viewportRows)) {
            const auto desired = cursorRow >= viewportRows
                                     ? cursorRow - viewportRows + 1
                                     : 0;
            if (location.line == firstVisibleLine()) {
                firstVisibleSubrow_ = desired;
                viewport()->update();
            } else {
                scrollToLine(location.line);
                firstVisibleSubrow_ = desired;
                viewport()->update();
            }
        }
    }

    const auto rowIndex = rowForByte(lineLayout,
                                     buffer_.lineRange(location.line).begin,
                                     offset);
    const auto cursorX = xForByte(
        lineLayout, buffer_.lineRange(location.line).begin, offset, rowIndex);
    const auto textWidth = std::max<qreal>(
        1, viewport()->width() - lineNumberGutterWidth());
    if (configuration_.wordWrap) return;
    if (cursorX < horizontalOffset_) setHorizontalOffset(cursorX);
    else if (cursorX > horizontalOffset_ + textWidth)
        setHorizontalOffset(cursorX - textWidth + 1);
}

void EditorRenderer::refreshFromModel() {
    updateScrollBar();
    if (configuration_.wordWrap) {
        firstVisibleSubrow_ = std::min(
            firstVisibleSubrow_,
            visualRowCount(firstVisibleLine()) - 1);
    } else {
        firstVisibleSubrow_ = 0;
    }
    invalidateViewport();
}

int EditorRenderer::effectiveLineHeight() const {
    if (configuration_.lineHeight > 0) return configuration_.lineHeight;
    return std::max(1, QFontMetrics(configuration_.font).height() + 2);
}

VisibleLineRange EditorRenderer::calculateVisibleLineRange() const noexcept {
    const auto lineCount = buffer_.lineCount();
    if (lineCount == 0) return {};
    const auto first = std::min(firstVisibleLine(), lineCount - 1);
    const auto rows = static_cast<std::size_t>(
        std::max(1, (viewport()->height() + effectiveLineHeight() - 1) /
                         effectiveLineHeight()));
    return {first, std::min(lineCount, first + rows + 1)};
}

qreal EditorRenderer::tabAdvance(qreal x) const {
    const QFontMetricsF metrics(configuration_.font);
    const auto space = metrics.horizontalAdvance(QLatin1Char(' '));
    const auto tab = std::max<qreal>(space, space * configuration_.tabWidth);
    const auto remainder = std::fmod(std::max<qreal>(0, x), tab);
    return tab - remainder;
}

EditorRenderer::LineLayout EditorRenderer::layoutLine(std::size_t line) const {
    const auto range = buffer_.lineRange(line);
    LineLayout layout;
    layout.source = buffer_.textRange(range.begin, range.end - range.begin);
    const auto& bytes = layout.source;
    const QFontMetricsF metrics(configuration_.font);
    layout.xAtByte.resize(bytes.size() + 1);
    qreal x = 0;
    std::size_t byte = 0;
    while (byte < bytes.size()) {
        layout.xAtByte[byte] = x;
        const auto next = byte + 1;
        const auto scalarEnd = [&] {
            auto candidate = next;
            while (candidate < bytes.size() &&
                   (static_cast<unsigned char>(bytes[candidate]) & 0xc0) == 0x80)
                ++candidate;
            return candidate;
        }();
        const auto scalar = QString::fromUtf8(
            bytes.data() + static_cast<std::ptrdiff_t>(byte),
            static_cast<qsizetype>(scalarEnd - byte));
        if (scalar == QLatin1String("\t")) {
            const auto advance = tabAdvance(x);
            const auto spaceWidth =
                std::max<qreal>(1, metrics.horizontalAdvance(QLatin1Char(' ')));
            const auto spaces = std::max(
                1, static_cast<int>(std::ceil(advance / spaceWidth - 0.001)));
            x += advance;
            layout.display += QString(spaces, QLatin1Char(' '));
        } else {
            x += metrics.horizontalAdvance(scalar);
            layout.display += scalar;
        }
        for (auto index = byte + 1; index < scalarEnd; ++index)
            layout.xAtByte[index] = x;
        byte = scalarEnd;
        layout.xAtByte[byte] = x;
    }
    layout.width = x;
    if (!configuration_.wordWrap) {
        layout.rows.push_back({0, bytes.size(), 0, layout.display.size(), x});
        if (!configuration_.showWhitespace &&
            !configuration_.matchingOccurrences &&
            !configuration_.indentationGuides)
            std::string{}.swap(layout.source);
        return layout;
    }

    const auto availableWidth = std::max<qreal>(
        1, viewport()->width() - lineNumberGutterWidth());
    const auto spaceWidth =
        std::max<qreal>(1, metrics.horizontalAdvance(QLatin1Char(' ')));
    std::size_t rowStart = 0;
    std::size_t lastBreak = 0;
    qsizetype rowStartDisplay = 0;
    qsizetype lastBreakDisplay = 0;
    qsizetype displayOffset = 0;
    byte = 0;
    while (byte < bytes.size()) {
        const auto next = byte + 1;
        auto scalarEnd = next;
        while (scalarEnd < bytes.size() &&
               (static_cast<unsigned char>(bytes[scalarEnd]) & 0xc0) == 0x80)
            ++scalarEnd;
        const auto rowWidth = layout.xAtByte[static_cast<int>(scalarEnd)] -
                              layout.xAtByte[static_cast<int>(rowStart)];
        if (scalarEnd > rowStart && rowWidth > availableWidth &&
            byte > rowStart) {
            const auto breakAt = lastBreak > rowStart ? lastBreak : byte;
            if (breakAt > rowStart) {
                const auto breakDisplay =
                    lastBreak > rowStart ? lastBreakDisplay : displayOffset;
                layout.rows.push_back(
                    {rowStart, breakAt,
                     rowStartDisplay, breakDisplay,
                     layout.xAtByte[static_cast<int>(breakAt)] -
                         layout.xAtByte[static_cast<int>(rowStart)]});
                rowStart = breakAt;
                rowStartDisplay = breakDisplay;
                lastBreak = 0;
                lastBreakDisplay = 0;
                byte = rowStart;
                displayOffset = rowStartDisplay;
                continue;
            }
        }
        const auto displayUnits = bytes[byte] == '\t'
            ? static_cast<qsizetype>(std::max(
                  1, static_cast<int>(std::ceil(
                         (layout.xAtByte[static_cast<int>(scalarEnd)] -
                          layout.xAtByte[static_cast<int>(byte)]) /
                             spaceWidth -
                         0.001))))
            : static_cast<qsizetype>(
                  static_cast<unsigned char>(bytes[byte]) >= 0xf0 ? 2 : 1);
        displayOffset += displayUnits;
        if (bytes[byte] == ' ' || bytes[byte] == '\t') {
            lastBreak = scalarEnd;
            lastBreakDisplay = displayOffset;
        }
        byte = scalarEnd;
    }
    layout.rows.push_back(
        {rowStart, bytes.size(),
         rowStartDisplay, displayOffset,
         layout.width - layout.xAtByte[static_cast<int>(rowStart)]});
    if (!configuration_.showWhitespace &&
        !configuration_.matchingOccurrences &&
        !configuration_.indentationGuides)
        std::string{}.swap(layout.source);
    return layout;
}

std::size_t EditorRenderer::rowForByte(const LineLayout& layout,
                                      std::size_t lineStart,
                                      std::size_t byteOffset) const {
    const auto local = std::min(
        byteOffset - lineStart,
        static_cast<std::size_t>(layout.xAtByte.size() - 1));
    const auto found = std::upper_bound(
        layout.rows.cbegin(), layout.rows.cend(), local,
        [](std::size_t value, const VisualRow& row) {
            return value < row.byteBegin;
        });
    if (found == layout.rows.cbegin()) return 0;
    return static_cast<std::size_t>(
        std::distance(layout.rows.cbegin(), found) - 1);
}

qreal EditorRenderer::xForByte(const LineLayout& layout, std::size_t lineStart,
                               std::size_t byteOffset,
                               std::size_t rowIndex) const {
    const auto& row = layout.rows[static_cast<int>(
        std::min(rowIndex, static_cast<std::size_t>(layout.rows.size() - 1)))];
    const auto local = std::clamp(
        byteOffset <= lineStart ? std::size_t{0} : byteOffset - lineStart,
        row.byteBegin, row.byteEnd);
    return layout.xAtByte[static_cast<int>(local)] -
           layout.xAtByte[static_cast<int>(row.byteBegin)];
}

std::pair<std::size_t, std::size_t> EditorRenderer::visibleByteWindow(
    const LineLayout& layout, std::size_t lineStart,
    std::size_t rowIndex) const {
    const auto& row = layout.rows[static_cast<int>(rowIndex)];
    const auto origin = layout.xAtByte[static_cast<int>(row.byteBegin)];
    const auto left = configuration_.wordWrap ? 0 : horizontalOffset_;
    const auto right = left + std::max<qreal>(
                                  1, viewport()->width() -
                                         lineNumberGutterWidth());
    const auto byteAtX = [&](qreal target) {
        const auto first = layout.xAtByte.cbegin() +
                           static_cast<qsizetype>(row.byteBegin);
        const auto last = layout.xAtByte.cbegin() +
                          static_cast<qsizetype>(row.byteEnd + 1);
        auto found = std::lower_bound(first, last, origin + target);
        auto local = row.byteBegin +
                     static_cast<std::size_t>(std::distance(first, found));
        local = std::min(local, row.byteEnd);
        while (local > row.byteBegin &&
               !buffer_.isScalarBoundary(lineStart + local))
            --local;
        while (local < row.byteEnd &&
               !buffer_.isScalarBoundary(lineStart + local))
            ++local;
        return local;
    };
    return {byteAtX(left), byteAtX(right)};
}

qreal EditorRenderer::visualRowTop(std::size_t line,
                                   std::size_t rowIndex) const {
    auto visualRow = -static_cast<std::ptrdiff_t>(firstVisibleSubrow_);
    for (auto current = firstVisibleLine(); current < line; ++current)
        visualRow += static_cast<std::ptrdiff_t>(visualRowCount(current));
    visualRow += static_cast<std::ptrdiff_t>(rowIndex);
    return static_cast<qreal>(visualRow * effectiveLineHeight());
}

QPointF EditorRenderer::textOrigin(std::size_t line,
                                   std::size_t rowIndex) const {
    return {textOriginX() - horizontalOffset_,
            visualRowTop(line, rowIndex)};
}

QRectF EditorRenderer::cursorGeometry(std::size_t cursorIndex) const {
    if (cursorIndex >= cursors_.cursorCount()) return {};
    const auto offset = cursorIndex == 0
                            ? cursors_.primaryOffset()
                            : cursors_.secondaryCursorOffset(cursorIndex - 1);
    const auto location = cursors_.lineAndColumn(offset);
    const auto visible = calculateVisibleLineRange();
    if (location.line < visible.begin || location.line >= visible.end) return {};
    const auto range = buffer_.lineRange(location.line);
    const auto layout = layoutLine(location.line);
    const auto row = rowForByte(layout, range.begin, offset);
    const auto origin = textOrigin(location.line, row);
    const QRectF geometry{origin.x() + xForByte(layout, range.begin, offset, row),
                          origin.y(), 1,
                          static_cast<qreal>(effectiveLineHeight())};
    const QRectF textRect{
        lineNumberGutterWidth(), 0,
        std::max<qreal>(0, viewport()->width() - lineNumberGutterWidth()),
        static_cast<qreal>(viewport()->height())};
    return geometry.intersects(textRect) ? geometry : QRectF{};
}

QVector<QRectF> EditorRenderer::selectionGeometry(std::size_t selectionIndex) const {
    QVector<QRectF> result;
    if (selectionIndex >= selections_.selectionCount() ||
        !selections_.hasSelection(selectionIndex))
        return result;
    const auto range = selections_.normalizedRange(selectionIndex);
    const auto visible = calculateVisibleLineRange();
    const auto lineHeight = effectiveLineHeight();
    qreal y = 0;
    for (std::size_t line = visible.begin; line < visible.end; ++line) {
        const auto lineRange = buffer_.lineRange(line);
        const auto layout = layoutLine(line);
        const auto firstRow = line == visible.begin ? firstVisibleSubrow_ : 0;
        for (auto rowIndex = firstRow;
             rowIndex < static_cast<std::size_t>(layout.rows.size()); ++rowIndex) {
            if (y >= viewport()->height() + lineHeight) break;
            const auto& row = layout.rows[static_cast<int>(rowIndex)];
            const auto rowBegin = lineRange.begin + row.byteBegin;
            const auto rowEnd = lineRange.begin + row.byteEnd;
            const auto start = std::max(range.begin, rowBegin);
            const auto end = std::min(range.end, rowEnd);
            const bool selectsLineBreak =
                rowIndex + 1 == static_cast<std::size_t>(layout.rows.size()) &&
                range.end > lineRange.begin &&
                range.begin <= rowEnd && range.end > rowEnd &&
                rowEnd == lineRange.end;
            if (!(range.end <= lineRange.begin || range.begin > lineRange.end) &&
                (start < end || selectsLineBreak)) {
                const auto xStart = xForByte(
                    layout, lineRange.begin, std::max(start, rowBegin), rowIndex);
                auto xEnd = xForByte(
                    layout, lineRange.begin, std::min(end, rowEnd), rowIndex);
                if (selectsLineBreak)
                    xEnd += QFontMetricsF(configuration_.font)
                                .horizontalAdvance(QLatin1Char(' '));
                const QRectF geometry{
                    textOriginX() - horizontalOffset_ + xStart, y,
                    std::max<qreal>(1, xEnd - xStart),
                    static_cast<qreal>(lineHeight)};
                const QRectF textRect{
                    lineNumberGutterWidth(), 0,
                    std::max<qreal>(0, viewport()->width() -
                                           lineNumberGutterWidth()),
                    static_cast<qreal>(viewport()->height())};
                const auto clipped = geometry.intersected(textRect);
                if (!clipped.isEmpty()) result.push_back(clipped);
            }
            y += lineHeight;
        }
        if (y >= viewport()->height() + lineHeight) break;
    }
    return result;
}

QVector<QRectF> EditorRenderer::matchingBracketGeometry() const {
    QVector<QRectF> result;
    if (!matchingBrackets_) return result;
    const auto visible = calculateVisibleLineRange();
    for (const auto offset : {matchingBrackets_->opening, matchingBrackets_->closing}) {
        if (offset >= buffer_.sizeBytes()) continue;
        const auto location = cursors_.lineAndColumn(offset);
        if (location.line < visible.begin || location.line >= visible.end) continue;
        const auto range = buffer_.lineRange(location.line);
        if (offset < range.begin || offset >= range.end) continue;
        const auto layout = layoutLine(location.line);
        const auto row = rowForByte(layout, range.begin, offset);
        const auto origin = textOrigin(location.line, row);
        const auto start = xForByte(layout, range.begin, offset, row);
        const auto end = xForByte(layout, range.begin, offset + 1, row);
        const QRectF geometry{
            origin.x() + start, origin.y(), std::max<qreal>(1, end - start),
            static_cast<qreal>(effectiveLineHeight())};
        const QRectF textRect{
            lineNumberGutterWidth(), 0,
            std::max<qreal>(0, viewport()->width() - lineNumberGutterWidth()),
            static_cast<qreal>(viewport()->height())};
        const auto clipped = geometry.intersected(textRect);
        if (!clipped.isEmpty()) result.push_back(clipped);
    }
    return result;
}

std::optional<std::pair<std::size_t, std::size_t>>
EditorRenderer::activeWordRange() const {
    if (buffer_.sizeBytes() == 0) return std::nullopt;
    auto offset = std::min(cursors_.primaryOffset(), buffer_.sizeBytes());
    if (offset == buffer_.sizeBytes() ||
        !isWordByte(buffer_.byteAt(offset))) {
        if (offset == 0) return std::nullopt;
        const auto previous = previousScalar(offset);
        if (!isWordByte(buffer_.byteAt(previous))) return std::nullopt;
        offset = previous;
    }

    auto begin = offset;
    while (begin > 0) {
        const auto previous = previousScalar(begin);
        if (!isWordByte(buffer_.byteAt(previous))) break;
        begin = previous;
    }
    auto end = nextScalar(offset);
    while (end < buffer_.sizeBytes() && isWordByte(buffer_.byteAt(end)))
        end = nextScalar(end);
    return std::pair{begin, end};
}

QVector<QRectF> EditorRenderer::matchingOccurrenceGeometry() const {
    QVector<QRectF> result;
    if (!configuration_.matchingOccurrences) return result;
    const auto target = activeWordRange();
    if (!target) return result;
    const auto word = buffer_.textRange(target->first,
                                        target->second - target->first);
    const auto visible = calculateVisibleLineRange();
    const auto lineHeight = effectiveLineHeight();
    const auto originX = textOriginX() - horizontalOffset_;
    qreal y = 0;
    for (auto line = visible.begin; line < visible.end; ++line) {
        const auto range = buffer_.lineRange(line);
        const auto layout = layoutLine(line);
        const auto firstRow = line == visible.begin ? firstVisibleSubrow_ : 0;
        for (auto rowIndex = firstRow;
             rowIndex < static_cast<std::size_t>(layout.rows.size()); ++rowIndex) {
            if (y >= viewport()->height() + lineHeight) break;
            const auto& row = layout.rows[static_cast<int>(rowIndex)];
            const auto contextBytes = word.size() - 1;
            const auto [visibleBegin, visibleEnd] =
                visibleByteWindow(layout, range.begin, rowIndex);
            auto search = visibleBegin -
                          std::min(visibleBegin - row.byteBegin, contextBytes);
            const auto searchEnd = visibleEnd +
                                   std::min(row.byteEnd - visibleEnd,
                                            contextBytes);
            while (search <= searchEnd) {
                const auto found = layout.source.find(word, search);
                if (found == std::string::npos || found > visibleEnd) break;
                const auto end = found + word.size();
                const bool leftBoundary =
                    found == 0 ||
                    !isWordByte(static_cast<unsigned char>(
                        layout.source[found - 1]));
                const bool rightBoundary =
                    end == layout.source.size() ||
                    !isWordByte(static_cast<unsigned char>(layout.source[end]));
                const auto start = std::max(found, row.byteBegin);
                const auto finish = std::min(end, row.byteEnd);
                if (leftBoundary && rightBoundary && start < finish) {
                    const auto xStart = xForByte(
                        layout, range.begin, range.begin + start, rowIndex);
                    const auto xEnd = xForByte(
                        layout, range.begin, range.begin + finish, rowIndex);
                    const QRectF geometry{
                        originX + xStart, y,
                        std::max<qreal>(1, xEnd - xStart),
                        static_cast<qreal>(lineHeight)};
                    const QRectF textRect{
                        lineNumberGutterWidth(), 0,
                        std::max<qreal>(0, viewport()->width() -
                                               lineNumberGutterWidth()),
                        static_cast<qreal>(viewport()->height())};
                    const auto clipped = geometry.intersected(textRect);
                    if (!clipped.isEmpty()) result.push_back(clipped);
                }
                search = std::max(end, found + 1);
            }
            y += lineHeight;
        }
        if (y >= viewport()->height() + lineHeight) break;
    }
    return result;
}

std::size_t EditorRenderer::nextScalar(std::size_t offset) const {
    if (offset >= buffer_.sizeBytes()) return buffer_.sizeBytes();
    ++offset;
    while (offset < buffer_.sizeBytes() && !buffer_.isScalarBoundary(offset))
        ++offset;
    return offset;
}

std::size_t EditorRenderer::previousScalar(std::size_t offset) const {
    if (offset == 0) return 0;
    --offset;
    while (offset > 0 && !buffer_.isScalarBoundary(offset)) --offset;
    return offset;
}

bool EditorRenderer::isWordByte(unsigned char value) noexcept {
    return value >= 0x80 || (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
           value == '_';
}

void EditorRenderer::paintEvent(QPaintEvent*) {
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), configuration_.background);
    painter.setFont(configuration_.font);
    lastPaintedLines_ = 0;
    lastPaintedVisualRows_ = 0;
    const auto visible = calculateVisibleLineRange();
    const auto lineHeight = effectiveLineHeight();
    const auto activeLine = cursors_.lineAndColumn(cursors_.primaryOffset()).line;
    const auto occurrence = configuration_.matchingOccurrences
                               ? activeWordRange()
                               : std::nullopt;
    const auto occurrenceText =
        occurrence ? buffer_.textRange(occurrence->first,
                                       occurrence->second - occurrence->first)
                   : std::string{};
    const auto gutter = lineNumberGutterWidth();
    const auto originX = gutter - horizontalOffset_;
    const auto viewportRows = static_cast<qreal>(viewport()->height());
    qreal y = 0;

    for (std::size_t line = visible.begin; line < visible.end; ++line) {
        const auto range = buffer_.lineRange(line);
        const auto layout = layoutLine(line);
        const auto firstRow = line == visible.begin ? firstVisibleSubrow_ : 0;
        bool paintedLine = false;
        for (auto rowIndex = firstRow;
             rowIndex < static_cast<std::size_t>(layout.rows.size()); ++rowIndex) {
            if (y >= viewportRows + lineHeight) break;
            const auto& row = layout.rows[static_cast<int>(rowIndex)];
            const QRectF rowRect{0, y, static_cast<qreal>(viewport()->width()),
                                 static_cast<qreal>(lineHeight)};
            painter.save();
            painter.setClipRect(rowRect.intersected(viewport()->rect()));

            if (configuration_.currentLineHighlight && line == activeLine) {
                painter.fillRect(rowRect, configuration_.currentLine);
            }
            painter.setClipRect(
                QRectF(gutter, y,
                       std::max<qreal>(0, viewport()->width() - gutter),
                       lineHeight),
                Qt::IntersectClip);

            if (!searchMatches_.isEmpty()) {
                const auto rowBegin = range.begin + row.byteBegin;
                const auto rowEnd = range.begin + row.byteEnd;
                for (const auto& match : searchMatches_) {
                    const auto begin = std::max(match.first, rowBegin);
                    const auto end = std::min(match.second, rowEnd);
                    if (begin >= end) continue;
                    const auto xStart = xForByte(
                        layout, range.begin, begin, rowIndex);
                    const auto xEnd = xForByte(
                        layout, range.begin, end, rowIndex);
                    painter.fillRect(QRectF(
                                         originX + xStart, y,
                                         std::max<qreal>(1, xEnd - xStart),
                                         lineHeight),
                                     configuration_.searchMatch);
                }
            }

            if (configuration_.matchingOccurrences && !occurrenceText.empty()) {
                const auto contextBytes = occurrenceText.size() - 1;
                const auto [visibleBegin, visibleEnd] =
                    visibleByteWindow(layout, range.begin, rowIndex);
                auto search =
                    visibleBegin -
                    std::min(visibleBegin - row.byteBegin, contextBytes);
                const auto searchEnd = visibleEnd +
                    std::min(row.byteEnd - visibleEnd, contextBytes);
                while (search <= searchEnd) {
                    const auto found = layout.source.find(occurrenceText, search);
                    if (found == std::string::npos || found > visibleEnd) break;
                    const auto end = found + occurrenceText.size();
                    const bool leftBoundary =
                        found == 0 ||
                        !isWordByte(static_cast<unsigned char>(
                            layout.source[found - 1]));
                    const bool rightBoundary =
                        end == layout.source.size() ||
                        !isWordByte(static_cast<unsigned char>(layout.source[end]));
                    const auto start = std::max(found, row.byteBegin);
                    const auto finish = std::min(end, row.byteEnd);
                    if (leftBoundary && rightBoundary && start < finish) {
                        const auto xStart = xForByte(
                            layout, range.begin, range.begin + start, rowIndex);
                        const auto xEnd = xForByte(
                            layout, range.begin, range.begin + finish, rowIndex);
                        painter.fillRect(QRectF(
                                             originX + xStart, y,
                                             std::max<qreal>(1, xEnd - xStart),
                                             lineHeight),
                                         configuration_.matchingOccurrence);
                    }
                    search = std::max(end, found + 1);
                }
            }

            for (std::size_t selection = 0;
                 selection < selections_.selectionCount(); ++selection) {
                if (!selections_.hasSelection(selection)) continue;
                const auto selected = selections_.normalizedRange(selection);
                if (selected.end <= range.begin || selected.begin > range.end)
                    continue;
                const auto rowBegin = range.begin + row.byteBegin;
                const auto rowEnd = range.begin + row.byteEnd;
                const auto start = std::max(selected.begin, rowBegin);
                const auto end = std::min(selected.end, rowEnd);
                const bool selectsLineBreak =
                    rowIndex + 1 == static_cast<std::size_t>(layout.rows.size()) &&
                    selected.begin <= rowEnd && selected.end > rowEnd &&
                    rowEnd == range.end;
                if (start >= end && !selectsLineBreak) continue;
                const auto xStart = xForByte(layout, range.begin,
                                             std::max(start, rowBegin), rowIndex);
                auto xEnd = xForByte(layout, range.begin,
                                     std::min(end, rowEnd), rowIndex);
                if (selectsLineBreak)
                    xEnd += QFontMetricsF(configuration_.font)
                                .horizontalAdvance(QLatin1Char(' '));
                painter.fillRect(QRectF(originX + xStart, y,
                                        std::max<qreal>(1, xEnd - xStart),
                                        lineHeight),
                                 configuration_.selection);
            }

            if (configuration_.indentationGuides) {
                std::size_t indentColumns = 0;
                for (std::size_t byte = 0; byte < layout.source.size(); ++byte) {
                    if (layout.source[byte] == ' ') {
                        ++indentColumns;
                    } else if (layout.source[byte] == '\t') {
                        indentColumns += static_cast<std::size_t>(
                            configuration_.tabWidth) -
                            indentColumns %
                                static_cast<std::size_t>(configuration_.tabWidth);
                    } else {
                        break;
                    }
                }
                const auto spaceWidth = QFontMetricsF(configuration_.font)
                                            .horizontalAdvance(QLatin1Char(' '));
                for (std::size_t column =
                         static_cast<std::size_t>(configuration_.tabWidth);
                     column <= indentColumns; column +=
                         static_cast<std::size_t>(configuration_.tabWidth)) {
                    const auto x = spaceWidth * static_cast<qreal>(column);
                    if (x >= viewport()->width() - gutter) break;
                    painter.setPen(configuration_.indentationGuide);
                    painter.drawLine(QPointF(originX + x, y),
                                     QPointF(originX + x, y + lineHeight));
                }
            }

            const auto displayLength = row.displayEnd - row.displayBegin;
            const auto displayText =
                layout.display.mid(row.displayBegin, displayLength);
            painter.setPen(configuration_.foreground);
            const QFontMetrics metrics(configuration_.font);
            const auto baseline = y + metrics.ascent() +
                std::max(0, (lineHeight - metrics.height()) / 2);
            painter.drawText(QPointF(originX, baseline), displayText);

            if (configuration_.showWhitespace) {
                for (auto byte = row.byteBegin; byte < row.byteEnd; ++byte) {
                    const auto value =
                        static_cast<unsigned char>(layout.source[byte]);
                    if (value != ' ' && value != '\t') continue;
                    const auto absolute = range.begin + byte;
                    const auto xStart = xForByte(layout, range.begin,
                                                 absolute, rowIndex);
                    const auto xEnd = xForByte(layout, range.begin,
                                               absolute + 1, rowIndex);
                    painter.setPen(configuration_.whitespace);
                    if (value == '\t') {
                        painter.drawText(QPointF(originX + xStart, baseline),
                                         QStringLiteral("→"));
                    } else {
                        painter.setBrush(configuration_.whitespace);
                        painter.drawEllipse(QPointF(
                            originX + (xStart + xEnd) / 2,
                            y + lineHeight / 2.0), 1.1, 1.1);
                    }
                }
                if (rowIndex + 1 ==
                        static_cast<std::size_t>(layout.rows.size()) &&
                    row.byteEnd == layout.source.size()) {
                    painter.setPen(configuration_.whitespace);
                    painter.drawText(QPointF(originX + row.width, baseline),
                                     QStringLiteral("↵"));
                }
            }

            if (matchingBrackets_) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(configuration_.matchingBracket));
                for (const auto offset : {matchingBrackets_->opening,
                                          matchingBrackets_->closing}) {
                    if (offset < range.begin + row.byteBegin ||
                        offset >= range.begin + row.byteEnd)
                        continue;
                    const auto xStart = xForByte(layout, range.begin, offset,
                                                 rowIndex);
                    const auto xEnd = xForByte(layout, range.begin, offset + 1,
                                               rowIndex);
                    painter.drawRect(QRectF(originX + xStart, y,
                                            std::max<qreal>(1, xEnd - xStart),
                                            std::max<qreal>(1, lineHeight - 1)));
                }
            }
            painter.restore();
            if (configuration_.lineNumbers && rowIndex == 0) {
                painter.setPen(line == activeLine
                                   ? configuration_.activeLineNumber
                                   : configuration_.lineNumber);
                painter.drawText(
                    QRectF(0, y, std::max<qreal>(
                                      0, gutter - QFontMetricsF(configuration_.font)
                                                     .horizontalAdvance(
                                                         QLatin1Char(' '))),
                           lineHeight),
                    Qt::AlignRight | Qt::AlignVCenter,
                    displayedLineNumber(line));
            }
            ++lastPaintedVisualRows_;
            paintedLine = paintedLine || rowRect.intersects(viewport()->rect());
            y += lineHeight;
        }
        if (paintedLine) ++lastPaintedLines_;
        if (y >= viewportRows + lineHeight) break;
    }
    painter.setBrush(configuration_.caret);
    painter.setPen(Qt::NoPen);
    for (std::size_t cursor = 0; cursor < cursors_.cursorCount(); ++cursor)
        painter.drawRect(cursorGeometry(cursor));
}

std::size_t EditorRenderer::lastPaintedLineCount() const noexcept {
    return lastPaintedLines_;
}

std::size_t EditorRenderer::lastPaintedVisualRowCount() const noexcept {
    return lastPaintedVisualRows_;
}

void EditorRenderer::updateScrollBar() {
    const auto rows = std::max(1, (viewport()->height() + effectiveLineHeight() - 1) /
                                      effectiveLineHeight());
    const auto lineCount = buffer_.lineCount();
    const auto maximum = configuration_.wordWrap
                             ? lineCount - 1
                             : (lineCount > static_cast<std::size_t>(rows)
                                    ? lineCount -
                                          static_cast<std::size_t>(rows)
                                    : 0);
    verticalScrollBar()->setRange(
        0, static_cast<int>(std::min<std::size_t>(
               maximum, static_cast<std::size_t>(std::numeric_limits<int>::max()))));
    verticalScrollBar()->setPageStep(rows);
}

void EditorRenderer::invalidateViewport() {
    viewport()->update();
}

void EditorRenderer::scrollByVisualRows(int rows) {
    if (!configuration_.wordWrap) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() + rows);
        return;
    }
    auto line = firstVisibleLine();
    auto rowCount = visualRowCount(line);
    while (rows > 0) {
        if (firstVisibleSubrow_ + 1 < rowCount) {
            ++firstVisibleSubrow_;
            --rows;
        } else if (line < buffer_.lineCount() - 1) {
            ++line;
            verticalScrollBar()->setValue(static_cast<int>(line));
            rowCount = visualRowCount(line);
            --rows;
        } else {
            break;
        }
    }
    while (rows < 0) {
        if (firstVisibleSubrow_ > 0) {
            --firstVisibleSubrow_;
            ++rows;
        } else if (line > 0) {
            --line;
            verticalScrollBar()->setValue(static_cast<int>(line));
            rowCount = visualRowCount(line);
            firstVisibleSubrow_ = rowCount - 1;
            ++rows;
        } else {
            break;
        }
    }
    viewport()->update();
}

void EditorRenderer::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBar();
    if (configuration_.wordWrap) {
        firstVisibleSubrow_ = std::min(
            firstVisibleSubrow_,
            visualRowCount(firstVisibleLine()) - 1);
    }
}

void EditorRenderer::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() != 0) {
        const auto steps = event->angleDelta().y() / 120;
        scrollByVisualRows(-steps);
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

} // namespace note::ui
