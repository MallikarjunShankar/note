#pragma once

#include "editor/cursor_manager.hpp"
#include "editor/editing_engine.hpp"
#include "editor/selection_manager.hpp"
#include "editor/undo_redo_manager.hpp"
#include "ui/editor_renderer.hpp"

#include <QWidget>

#include <cstddef>
#include <functional>

class QEvent;
class QMouseEvent;

namespace note::ui {

class EditorView final : public QWidget {
    Q_OBJECT
public:
    explicit EditorView(editor::TextBuffer& buffer,
                        editor::LineEnding preferredLineEnding,
                        QWidget* parent = nullptr);

    [[nodiscard]] editor::TextBuffer& buffer() const noexcept;
    [[nodiscard]] editor::CursorManager& cursors() noexcept;
    [[nodiscard]] editor::SelectionManager& selections() noexcept;
    [[nodiscard]] EditorRenderer* renderer() const noexcept;
    void setDocumentChangedCallback(std::function<void()> callback);
    void refreshFromModel();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool handleKeyPress(class QKeyEvent* event);
    bool handleMousePress(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event);
    bool handleMouseDoubleClick(QMouseEvent* event);
    void moveCursor(int key, bool extendSelection);
    void setPrimarySelection(std::size_t anchor, std::size_t active);
    void collapseSelectionsAt(std::size_t offset);
    [[nodiscard]] bool hasAnySelection() const;
    void afterEdit();
    void insertNewlineWithIndent();
    void copySelection(bool cut);
    void pasteClipboard();
    void selectWord(std::size_t offset);
    [[nodiscard]] std::size_t previousScalar(std::size_t offset) const;
    [[nodiscard]] std::size_t nextScalar(std::size_t offset) const;

    editor::TextBuffer& buffer_;
    editor::CursorManager cursors_;
    editor::SelectionManager selections_;
    editor::EditingEngine editing_;
    editor::UndoRedoManager history_;
    editor::LineEnding preferredLineEnding_;
    EditorRenderer* renderer_{};
    std::function<void()> documentChangedCallback_;
    std::size_t dragAnchor_{};
    bool dragging_{};
    bool extendingSelection_{};
};

} // namespace note::ui
