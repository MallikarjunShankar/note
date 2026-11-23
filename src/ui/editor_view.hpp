#pragma once

#include "editor/bracket_manager.hpp"
#include "editor/cursor_manager.hpp"
#include "editor/editing_engine.hpp"
#include "editor/selection_manager.hpp"
#include "editor/undo_redo_manager.hpp"
#include "ui/editor_renderer.hpp"

#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

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
    [[nodiscard]] editor::BracketManager& brackets() noexcept;
    [[nodiscard]] EditorRenderer* renderer() const noexcept;
    void setDocumentChangedCallback(std::function<void()> callback);
    void refreshFromModel();
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    void beginUndoTransaction();
    void endUndoTransaction();
    void replaceRange(std::size_t offset, std::size_t length,
                      std::string_view utf8Text);
    void selectRange(std::size_t begin, std::size_t end);
    void setSearchMatches(const std::vector<std::pair<std::size_t, std::size_t>>& matches);
    void setBracketConfiguration(editor::BracketConfiguration configuration);
    void restoreCursorPosition(std::size_t offset);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool handleKeyPress(class QKeyEvent* event);
    bool handleMousePress(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event);
    bool handleMouseDoubleClick(QMouseEvent* event);
    bool handleTextCharacter(char character);
    void moveCursor(int key, bool extendSelection);
    void setPrimarySelection(std::size_t anchor, std::size_t active);
    void collapseSelectionsAt(std::size_t offset);
    void collapseSelectionsAtCursors();
    void ensureSelectionAlignment();
    void normalizeCursorSelections();
    void setCursorOffset(std::size_t index, std::size_t offset);
    [[nodiscard]] std::size_t cursorOffset(std::size_t index) const;
    [[nodiscard]] bool hasAnySelection() const;
    void afterEdit();
    void updateMatchingBrackets(bool force = false);
    void backspaceWithPairHandling();
    void insertNewlineWithIndent();
    void copySelection(bool cut);
    void pasteClipboard();
    void selectWord(std::size_t offset);
    [[nodiscard]] std::size_t previousScalar(std::size_t offset) const;
    [[nodiscard]] std::size_t nextScalar(std::size_t offset) const;

    editor::TextBuffer& buffer_;
    editor::CursorManager cursors_;
    editor::SelectionManager selections_;
    editor::BracketManager brackets_;
    editor::EditingEngine editing_;
    editor::UndoRedoManager history_;
    editor::LineEnding preferredLineEnding_;
    EditorRenderer* renderer_{};
    std::function<void()> documentChangedCallback_;
    std::size_t dragAnchor_{};
    bool dragging_{};
    bool extendingSelection_{};
    std::size_t lastMatchCursor_{static_cast<std::size_t>(-1)};
    std::uint64_t lastMatchRevision_{static_cast<std::uint64_t>(-1)};
};

} // namespace note::ui
