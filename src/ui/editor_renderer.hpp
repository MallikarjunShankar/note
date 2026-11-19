#pragma once

#include "editor/bracket_manager.hpp"
#include "editor/selection_manager.hpp"

#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QRectF>
#include <QVector>

#include <cstddef>
#include <optional>

namespace note::editor {
class CursorManager;
class TextBuffer;
}

namespace note::ui {

struct EditorRenderConfiguration {
    QFont font;
    int tabWidth{4};
    int lineHeight{};
    QColor foreground{QStringLiteral("#d7dee9")};
    QColor background{QStringLiteral("#1e222a")};
    QColor selection{QStringLiteral("#3d5a80")};
    QColor caret{QStringLiteral("#f2f5f8")};
    QColor matchingBracket{QStringLiteral("#66788a")};
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
    void scrollToLine(std::size_t line);
    void setHorizontalOffset(qreal offset);
    [[nodiscard]] qreal horizontalOffset() const noexcept;
    [[nodiscard]] std::size_t offsetAtPosition(const QPointF& position) const;
    void ensureCursorVisible(std::size_t offset);
    void refreshFromModel();
    void setMatchingBrackets(std::optional<editor::BracketPair> pair);

    [[nodiscard]] QRectF cursorGeometry(std::size_t cursorIndex) const;
    [[nodiscard]] QVector<QRectF> selectionGeometry(std::size_t selectionIndex) const;
    [[nodiscard]] QVector<QRectF> matchingBracketGeometry() const;
    [[nodiscard]] std::size_t lastPaintedLineCount() const noexcept;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct LineLayout {
        QString display;
        QVector<qreal> xAtByte;
        qreal width{};
    };

    [[nodiscard]] int effectiveLineHeight() const;
    [[nodiscard]] VisibleLineRange calculateVisibleLineRange() const noexcept;
    [[nodiscard]] LineLayout layoutLine(std::size_t line) const;
    [[nodiscard]] qreal xForByte(const LineLayout& layout,
                                 std::size_t lineStart,
                                 std::size_t byteOffset) const;
    [[nodiscard]] qreal tabAdvance(qreal x) const;
    [[nodiscard]] QPointF textOrigin(std::size_t line) const;
    void updateScrollBar();
    void invalidateViewport();

    const editor::TextBuffer& buffer_;
    const editor::CursorManager& cursors_;
    const editor::SelectionManager& selections_;
    EditorRenderConfiguration configuration_;
    qreal horizontalOffset_{};
    std::size_t lastPaintedLines_{};
    std::optional<editor::BracketPair> matchingBrackets_;
};

} // namespace note::ui
