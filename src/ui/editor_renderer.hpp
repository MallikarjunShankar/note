#pragma once

#include "editor/bracket_manager.hpp"
#include "editor/selection_manager.hpp"

#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QRectF>
#include <QTimer>
#include <QVector>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace note::editor {
class CursorManager;
class TextBuffer;
}

namespace note::ui {

struct EditorRenderConfiguration {
    QFont font;
    int tabWidth{4};
    int lineHeight{};
    bool lineNumbers{true};
    bool relativeLineNumbers{};
    bool currentLineHighlight{true};
    bool indentationGuides{};
    bool showWhitespace{};
    bool matchingOccurrences{};
    bool wordWrap{};
    QColor foreground{QStringLiteral("#d7dee9")};
    QColor background{QStringLiteral("#1e222a")};
    QColor selection{QStringLiteral("#3d5a80")};
    QColor caret{QStringLiteral("#f2f5f8")};
    QColor matchingBracket{QStringLiteral("#66788a")};
    QColor currentLine{QStringLiteral("#252b35")};
    QColor lineNumber{QStringLiteral("#737d8c")};
    QColor activeLineNumber{QStringLiteral("#d7dee9")};
    QColor indentationGuide{QStringLiteral("#39414d")};
    QColor whitespace{QStringLiteral("#687382")};
    QColor matchingOccurrence{QStringLiteral("#3a4655")};
    QColor searchMatch{QStringLiteral("#6b5523")};
};

struct VisibleLineRange {
    std::size_t begin{};
    std::size_t end{}; // Exclusive.
};

class EditorRenderer final : public QAbstractScrollArea {
    Q_OBJECT
public:
    EditorRenderer(const editor::TextBuffer& buffer,
                   const editor::CursorManager& cursors,
                   const editor::SelectionManager& selections,
                   QWidget* parent = nullptr);

    [[nodiscard]] const EditorRenderConfiguration& configuration() const noexcept;
    void setConfiguration(EditorRenderConfiguration configuration);

    [[nodiscard]] VisibleLineRange visibleLineRange() const noexcept;
    [[nodiscard]] std::size_t firstVisibleLine() const noexcept;
    [[nodiscard]] std::size_t firstVisibleSubrow() const noexcept;
    [[nodiscard]] qreal lineNumberGutterWidth() const;
    [[nodiscard]] qreal textOriginX() const;
    [[nodiscard]] QString displayedLineNumber(std::size_t line) const;
    [[nodiscard]] std::size_t visualRowCount(std::size_t line) const;
    [[nodiscard]] std::size_t totalVisualRowCount() const noexcept;
    [[nodiscard]] bool wrappedRowIndexComplete() const noexcept;
    void scrollToLine(std::size_t line);
    void setHorizontalOffset(qreal offset);
    [[nodiscard]] qreal horizontalOffset() const noexcept;
    [[nodiscard]] std::size_t offsetAtPosition(const QPointF& position) const;
    void ensureCursorVisible(std::size_t offset);
    void refreshFromModel();
    void setMatchingBrackets(std::optional<editor::BracketPair> pair);
    void setSearchMatches(const QVector<QPair<std::size_t, std::size_t>>& matches);

    [[nodiscard]] QRectF cursorGeometry(std::size_t cursorIndex) const;
    [[nodiscard]] QVector<QRectF> selectionGeometry(std::size_t selectionIndex) const;
    [[nodiscard]] QVector<QRectF> matchingBracketGeometry() const;
    [[nodiscard]] QVector<QRectF> matchingOccurrenceGeometry() const;
    [[nodiscard]] std::size_t lastPaintedLineCount() const noexcept;
    [[nodiscard]] std::size_t lastPaintedVisualRowCount() const noexcept;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct VisualRow {
        std::size_t byteBegin{};
        std::size_t byteEnd{};
        qsizetype displayBegin{};
        qsizetype displayEnd{};
        qreal width{};
    };

    struct LineLayout {
        std::string source;
        QString display;
        QVector<qreal> xAtByte;
        QVector<VisualRow> rows;
        qreal width{};
    };

    [[nodiscard]] int effectiveLineHeight() const;
    [[nodiscard]] VisibleLineRange calculateVisibleLineRange() const noexcept;
    [[nodiscard]] LineLayout layoutLine(std::size_t line) const;
    [[nodiscard]] std::size_t rowForByte(const LineLayout& layout,
                                         std::size_t lineStart,
                                         std::size_t byteOffset) const;
    [[nodiscard]] qreal xForByte(const LineLayout& layout,
                                 std::size_t lineStart,
                                 std::size_t byteOffset,
                                 std::size_t rowIndex) const;
    [[nodiscard]] std::pair<std::size_t, std::size_t> visibleByteWindow(
        const LineLayout& layout, std::size_t lineStart,
        std::size_t rowIndex) const;
    [[nodiscard]] qreal tabAdvance(qreal x) const;
    [[nodiscard]] qreal visualRowTop(std::size_t line,
                                     std::size_t rowIndex) const;
    [[nodiscard]] QPointF textOrigin(std::size_t line,
                                     std::size_t rowIndex) const;
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
    activeWordRange() const;
    [[nodiscard]] std::size_t nextScalar(std::size_t offset) const;
    [[nodiscard]] std::size_t previousScalar(std::size_t offset) const;
    [[nodiscard]] static bool isWordByte(unsigned char value) noexcept;
    void scrollByVisualRows(int rows);
    void updateScrollBar();
    void resetWrappedRowIndex();
    void processWrappedRowIndexChunk();
    void updateWrappedRowCount(std::size_t line, std::size_t count);
    [[nodiscard]] std::size_t wrappedRowsBefore(std::size_t line) const noexcept;
    [[nodiscard]] std::size_t lineAtWrappedRow(std::size_t row) const noexcept;
    void setWrappedScrollPosition(std::size_t line, std::size_t subrow);
    void invalidateViewport();

    const editor::TextBuffer& buffer_;
    const editor::CursorManager& cursors_;
    const editor::SelectionManager& selections_;
    EditorRenderConfiguration configuration_;
    qreal horizontalOffset_{};
    std::size_t firstVisibleLine_{};
    std::size_t firstVisibleSubrow_{};
    std::vector<std::size_t> wrappedRowsByLine_;
    std::vector<std::size_t> wrappedRowsTree_;
    std::size_t wrappedRowsIndexed_{};
    std::size_t totalVisualRows_{};
    std::size_t wrappedIndexWidth_{};
    std::uint64_t observedBufferRevision_{};
    bool wrappedIndexComplete_{};
    QTimer wrappedIndexTimer_;
    std::size_t lastPaintedLines_{};
    std::size_t lastPaintedVisualRows_{};
    std::optional<editor::BracketPair> matchingBrackets_;
    QVector<QPair<std::size_t, std::size_t>> searchMatches_;
};

} // namespace note::ui
