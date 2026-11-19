#include "ui/editor_renderer.hpp"

#include "editor/cursor_manager.hpp"
#include "editor/text_buffer.hpp"

#include <QFontDatabase>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <stdexcept>

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
            [this] { viewport()->update(); });
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
    updateScrollBar();
    invalidateViewport();
}

VisibleLineRange EditorRenderer::visibleLineRange() const noexcept {
    return calculateVisibleLineRange();
}

std::size_t EditorRenderer::firstVisibleLine() const noexcept {
    return static_cast<std::size_t>(verticalScrollBar()->value());
}

void EditorRenderer::scrollToLine(std::size_t line) {
    const auto maximum = verticalScrollBar()->maximum();
    verticalScrollBar()->setValue(static_cast<int>(
        std::min<std::size_t>(line, static_cast<std::size_t>(maximum))));
}

void EditorRenderer::setHorizontalOffset(qreal offset) {
    horizontalOffset_ = std::max<qreal>(0, offset);
    invalidateViewport();
}

qreal EditorRenderer::horizontalOffset() const noexcept {
    return horizontalOffset_;
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
    const auto bytes = buffer_.textRange(range.begin, range.end - range.begin);
    const QFontMetricsF metrics(configuration_.font);
    LineLayout layout;
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
            x += tabAdvance(x);
            layout.display += QString(configuration_.tabWidth, QLatin1Char(' '));
        } else {
            x += metrics.horizontalAdvance(scalar);
            layout.display += scalar;
        }
        for (auto index = byte + 1; index < scalarEnd; ++index)
            layout.xAtByte[index] = x;
        byte = scalarEnd;
    }
    layout.xAtByte[bytes.size()] = x;
    layout.width = x;
    return layout;
}

qreal EditorRenderer::xForByte(const LineLayout& layout, std::size_t lineStart,
                               std::size_t byteOffset) const {
    if (byteOffset <= lineStart) return 0;
    const auto local = std::min(byteOffset - lineStart,
                                static_cast<std::size_t>(layout.xAtByte.size() - 1));
    return layout.xAtByte[static_cast<int>(local)];
}

QPointF EditorRenderer::textOrigin(std::size_t line) const {
    const auto lineDelta = static_cast<qreal>(line) -
                           static_cast<qreal>(firstVisibleLine());
    return {static_cast<qreal>(-horizontalOffset_),
            lineDelta * static_cast<qreal>(effectiveLineHeight())};
}

QRectF EditorRenderer::cursorGeometry(std::size_t cursorIndex) const {
    if (cursorIndex >= cursors_.cursorCount()) return {};
    const auto offset = cursorIndex == 0
                            ? cursors_.primaryOffset()
                            : cursors_.secondaryCursorOffset(cursorIndex - 1);
    const auto location = cursors_.lineAndColumn(offset);
    const auto range = buffer_.lineRange(location.line);
    const auto layout = layoutLine(location.line);
    const auto origin = textOrigin(location.line);
    return {origin.x() + xForByte(layout, range.begin, offset),
            origin.y(), 1, static_cast<qreal>(effectiveLineHeight())};
}

QVector<QRectF> EditorRenderer::selectionGeometry(std::size_t selectionIndex) const {
    QVector<QRectF> result;
    if (selectionIndex >= selections_.selectionCount() ||
        !selections_.hasSelection(selectionIndex))
        return result;
    const auto range = selections_.normalizedRange(selectionIndex);
    const auto visible = calculateVisibleLineRange();
    for (std::size_t line = visible.begin; line < visible.end; ++line) {
        const auto lineRange = buffer_.lineRange(line);
        if (range.end <= lineRange.begin || range.begin > lineRange.end) continue;
        const auto start = std::max(range.begin, lineRange.begin);
        const auto end = std::min(range.end, lineRange.end);
        const auto layout = layoutLine(line);
        auto xStart = xForByte(layout, lineRange.begin, start);
        auto xEnd = xForByte(layout, lineRange.begin, end);
        if (xStart == xEnd && range.end > lineRange.end)
            xEnd += QFontMetricsF(configuration_.font).horizontalAdvance(QLatin1Char(' '));
        const auto origin = textOrigin(line);
        result.push_back({origin.x() + xStart, origin.y(),
                          std::max<qreal>(1, xEnd - xStart),
                          static_cast<qreal>(effectiveLineHeight())});
    }
    return result;
}

void EditorRenderer::paintEvent(QPaintEvent*) {
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), configuration_.background);
    painter.setFont(configuration_.font);
    lastPaintedLines_ = 0;
    const auto visible = calculateVisibleLineRange();
    for (std::size_t line = visible.begin; line < visible.end; ++line) {
        const auto range = buffer_.lineRange(line);
        const auto layout = layoutLine(line);
        const auto origin = textOrigin(line);
        const QRectF lineRect{0, origin.y(), static_cast<qreal>(viewport()->width()),
                              static_cast<qreal>(effectiveLineHeight())};
        painter.save();
        painter.setClipRect(lineRect);
        for (std::size_t selection = 0; selection < selections_.selectionCount();
             ++selection) {
            painter.setBrush(configuration_.selection);
            painter.setPen(Qt::NoPen);
            for (const auto& rectangle : selectionGeometry(selection))
                painter.drawRect(rectangle);
        }
        painter.setPen(configuration_.foreground);
        painter.drawText(origin, layout.display);
        painter.restore();
        ++lastPaintedLines_;
        (void)range;
    }
    painter.setBrush(configuration_.caret);
    painter.setPen(Qt::NoPen);
    for (std::size_t cursor = 0; cursor < cursors_.cursorCount(); ++cursor)
        painter.drawRect(cursorGeometry(cursor));
}

std::size_t EditorRenderer::lastPaintedLineCount() const noexcept {
    return lastPaintedLines_;
}

void EditorRenderer::updateScrollBar() {
    const auto rows = std::max(1, (viewport()->height() + effectiveLineHeight() - 1) /
                                      effectiveLineHeight());
    const auto lineCount = buffer_.lineCount();
    const auto maximum = lineCount > static_cast<std::size_t>(rows)
                             ? lineCount - static_cast<std::size_t>(rows)
                             : 0;
    verticalScrollBar()->setRange(
        0, static_cast<int>(std::min<std::size_t>(
               maximum, static_cast<std::size_t>(std::numeric_limits<int>::max()))));
    verticalScrollBar()->setPageStep(rows);
}

void EditorRenderer::invalidateViewport() {
    viewport()->update();
}

void EditorRenderer::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBar();
}

void EditorRenderer::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() != 0) {
        const auto steps = event->angleDelta().y() / 120;
        verticalScrollBar()->setValue(verticalScrollBar()->value() - steps);
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

} // namespace note::ui
