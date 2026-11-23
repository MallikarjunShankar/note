#pragma once

#include "editor/editing_engine.hpp"
#include "editor/selection_manager.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace note::editor {

class CursorManager;

class UndoRedoManager final {
public:
    UndoRedoManager(TextBuffer& buffer, CursorManager& cursors,
                    SelectionManager& selections, EditingEngine& editing);

    [[nodiscard]] EditBatch insertText(
        std::string_view utf8Text, LineEnding bareLineEnding = LineEnding::lf);
    [[nodiscard]] EditBatch insertTextAtCursorIndices(
        std::span<const std::size_t> cursorIndices, std::string_view utf8Text,
        LineEnding bareLineEnding = LineEnding::lf);
    [[nodiscard]] EditBatch backspace();
    [[nodiscard]] EditBatch deleteForward();
    [[nodiscard]] EditBatch insertNewline(
        LineEnding lineEnding = LineEnding::lf);
    [[nodiscard]] EditBatch replaceSelections(
        std::string_view utf8Text, LineEnding bareLineEnding = LineEnding::lf);
    [[nodiscard]] EditBatch deleteSelections();
    [[nodiscard]] EditBatch replaceRange(
        std::size_t offset, std::size_t length,
        std::string_view utf8Text,
        LineEnding bareLineEnding = LineEnding::lf);
    [[nodiscard]] EditBatch indentLines(bool useTabs, std::size_t width);
    [[nodiscard]] EditBatch outdentLines(std::size_t width);

    void beginTransaction();
    void endTransaction();
    void clearHistory() noexcept;
    bool undo();
    bool redo();

    void setMemoryLimit(std::size_t bytes);
    [[nodiscard]] std::size_t memoryLimit() const noexcept;
    [[nodiscard]] std::size_t memoryUsage() const noexcept;
    [[nodiscard]] std::size_t undoCount() const noexcept;
    [[nodiscard]] std::size_t redoCount() const noexcept;

private:
    enum class OperationKind { other, typing, backspace };

    struct EditorState {
        std::vector<std::size_t> cursors;
        std::vector<Selection> selections;
    };

    struct Transaction {
        std::vector<EditBatch> batches;
        EditorState before;
        EditorState after;
        OperationKind kind{OperationKind::other};
        std::size_t bytes{};
    };

    [[nodiscard]] EditorState captureState() const;
    void restoreState(const EditorState& state);
    [[nodiscard]] EditBatch finish(EditBatch batch, OperationKind kind,
                                    const EditorState& before);
    void appendToTransaction(Transaction& transaction, EditBatch batch,
                             OperationKind kind, const EditorState& after);
    void commit(Transaction transaction);
    [[nodiscard]] bool canGroup(const Transaction& transaction,
                                OperationKind kind,
                                const EditorState& before) const noexcept;
    [[nodiscard]] bool sameState(const EditorState& left,
                                 const EditorState& right) const noexcept;
    [[nodiscard]] std::size_t transactionBytes(const Transaction& transaction) const;
    void applyBatch(const EditBatch& batch, bool inverse);
    void applyTransaction(const Transaction& transaction, bool inverse);
    [[nodiscard]] OperationKind classifyTyping(const EditBatch& batch) const;
    void evictToLimit() noexcept;

    TextBuffer& buffer_;
    CursorManager& cursors_;
    SelectionManager& selections_;
    EditingEngine& editing_;
    std::vector<Transaction> undoHistory_;
    std::vector<Transaction> redoHistory_;
    std::optional<Transaction> openTransaction_;
    std::size_t memoryLimit_{8 * 1024 * 1024};
    std::size_t memoryUsage_{};
};

} // namespace note::editor
