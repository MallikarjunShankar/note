#include "editor/undo_redo_manager.hpp"

#include "editor/cursor_manager.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace note::editor {
namespace {

bool singleScalar(std::string_view text) {
    if (text.empty()) return false;
    TextBuffer value(text);
    return value.sizeBytes() == text.size() && value.lineCount() == 1 &&
           value.isScalarBoundary(1);
}

void addBytes(std::size_t& total, std::size_t amount) {
    if (amount > std::numeric_limits<std::size_t>::max() - total)
        total = std::numeric_limits<std::size_t>::max();
    else
        total += amount;
}

} // namespace

UndoRedoManager::UndoRedoManager(TextBuffer& buffer, CursorManager& cursors,
                                 SelectionManager& selections,
                                 EditingEngine& editing)
    : buffer_(buffer), cursors_(cursors), selections_(selections),
      editing_(editing) {}

EditBatch UndoRedoManager::insertText(std::string_view utf8Text,
                                       LineEnding bareLineEnding) {
    const auto before = captureState();
    auto batch = editing_.insertText(utf8Text, bareLineEnding);
    const auto kind = classifyTyping(batch);
    return finish(std::move(batch), kind, before);
}

EditBatch UndoRedoManager::backspace() {
    const auto before = captureState();
    auto batch = editing_.backspace();
    return finish(std::move(batch), OperationKind::backspace, before);
}

EditBatch UndoRedoManager::deleteForward() {
    const auto before = captureState();
    auto batch = editing_.deleteForward();
    return finish(std::move(batch), OperationKind::other, before);
}

EditBatch UndoRedoManager::insertNewline(LineEnding lineEnding) {
    const auto before = captureState();
    auto batch = editing_.insertNewline(lineEnding);
    return finish(std::move(batch), OperationKind::other, before);
}

EditBatch UndoRedoManager::replaceSelections(std::string_view utf8Text,
                                              LineEnding bareLineEnding) {
    const auto before = captureState();
    auto batch = editing_.replaceSelections(utf8Text, bareLineEnding);
    return finish(std::move(batch), OperationKind::other, before);
}

EditBatch UndoRedoManager::deleteSelections() {
    const auto before = captureState();
    auto batch = editing_.deleteSelections();
    return finish(std::move(batch), OperationKind::other, before);
}

void UndoRedoManager::beginTransaction() {
    if (openTransaction_) throw std::logic_error("undo transaction already open");
    openTransaction_ = Transaction{};
    openTransaction_->before = captureState();
}

void UndoRedoManager::endTransaction() {
    if (!openTransaction_) throw std::logic_error("no undo transaction is open");
    if (!openTransaction_->batches.empty()) commit(std::move(*openTransaction_));
    openTransaction_.reset();
}

void UndoRedoManager::clearHistory() noexcept {
    undoHistory_.clear();
    redoHistory_.clear();
    openTransaction_.reset();
    memoryUsage_ = 0;
}

bool UndoRedoManager::undo() {
    if (undoHistory_.empty()) return false;
    auto transaction = std::move(undoHistory_.back());
    undoHistory_.pop_back();
    memoryUsage_ -= transaction.bytes;
    applyTransaction(transaction, true);
    memoryUsage_ += transaction.bytes;
    redoHistory_.push_back(std::move(transaction));
    return true;
}

bool UndoRedoManager::redo() {
    if (redoHistory_.empty()) return false;
    auto transaction = std::move(redoHistory_.back());
    redoHistory_.pop_back();
    memoryUsage_ -= transaction.bytes;
    applyTransaction(transaction, false);
    memoryUsage_ += transaction.bytes;
    undoHistory_.push_back(std::move(transaction));
    evictToLimit();
    return true;
}

void UndoRedoManager::setMemoryLimit(std::size_t bytes) {
    memoryLimit_ = bytes;
    evictToLimit();
}

std::size_t UndoRedoManager::memoryLimit() const noexcept { return memoryLimit_; }
std::size_t UndoRedoManager::memoryUsage() const noexcept { return memoryUsage_; }
std::size_t UndoRedoManager::undoCount() const noexcept { return undoHistory_.size(); }
std::size_t UndoRedoManager::redoCount() const noexcept { return redoHistory_.size(); }

UndoRedoManager::EditorState UndoRedoManager::captureState() const {
    EditorState state;
    state.cursors.reserve(cursors_.cursorCount());
    state.cursors.push_back(cursors_.primaryOffset());
    for (const auto offset : cursors_.secondaryCursorOffsets())
        state.cursors.push_back(offset);
    state.selections.reserve(selections_.selectionCount());
    for (std::size_t index = 0; index < selections_.selectionCount(); ++index)
        state.selections.push_back(selections_.selection(index));
    return state;
}

void UndoRedoManager::restoreState(const EditorState& state) {
    if (state.cursors.empty()) throw std::logic_error("cursor state is empty");
    cursors_.setPrimaryCursor(state.cursors.front());
    cursors_.clearSecondaryCursors();
    for (std::size_t index = 1; index < state.cursors.size(); ++index) {
        if (!cursors_.addSecondaryCursor(state.cursors[index]))
            throw std::logic_error("cursor state contains duplicate positions");
    }
    selections_.clearSelections();
    for (const auto& selection : state.selections)
        selections_.addSelection(selection.anchor, selection.active);
}

EditBatch UndoRedoManager::finish(EditBatch batch, OperationKind kind,
                                   const EditorState& before) {
    if (batch.empty()) return batch;
    const auto after = captureState();
    if (openTransaction_) {
        appendToTransaction(*openTransaction_, std::move(batch), kind, after);
    } else {
        Transaction transaction;
        transaction.before = before;
        transaction.kind = kind;
        transaction.batches.push_back(std::move(batch));
        transaction.after = after;
        transaction.bytes = transactionBytes(transaction);
        if (canGroup(undoHistory_.empty() ? Transaction{} : undoHistory_.back(),
                     kind, before) &&
            !undoHistory_.empty()) {
            auto candidate = undoHistory_.back();
            candidate.batches.push_back(transaction.batches.back());
            candidate.after = after;
            candidate.bytes = transactionBytes(candidate);
            if (candidate.bytes <= memoryLimit_) {
                memoryUsage_ -= undoHistory_.back().bytes;
                undoHistory_.back() = std::move(candidate);
                memoryUsage_ += undoHistory_.back().bytes;
                for (const auto& redo : redoHistory_) memoryUsage_ -= redo.bytes;
                redoHistory_.clear();
                return batch;
            }
        }
        commit(std::move(transaction));
    }
    return batch;
}

void UndoRedoManager::appendToTransaction(Transaction& transaction, EditBatch batch,
                                           OperationKind kind,
                                           const EditorState& after) {
    transaction.batches.push_back(std::move(batch));
    transaction.kind = kind;
    transaction.after = after;
    transaction.bytes = transactionBytes(transaction);
}

void UndoRedoManager::commit(Transaction transaction) {
    transaction.bytes = transactionBytes(transaction);
    for (const auto& redo : redoHistory_) memoryUsage_ -= redo.bytes;
    redoHistory_.clear();
    if (transaction.bytes > memoryLimit_) return;
    memoryUsage_ += transaction.bytes;
    undoHistory_.push_back(std::move(transaction));
    evictToLimit();
}

bool UndoRedoManager::canGroup(const Transaction& transaction,
                               OperationKind kind,
                               const EditorState& before) const noexcept {
    return !transaction.batches.empty() &&
           transaction.kind == kind &&
           sameState(transaction.after, before) &&
           (kind == OperationKind::typing || kind == OperationKind::backspace);
}

bool UndoRedoManager::sameState(const EditorState& left,
                                const EditorState& right) const noexcept {
    if (left.cursors != right.cursors || left.selections.size() != right.selections.size())
        return false;
    return std::equal(left.selections.begin(), left.selections.end(),
                      right.selections.begin(),
                      [](const Selection& first, const Selection& second) {
                          return first.anchor == second.anchor &&
                                 first.active == second.active;
                      });
}

std::size_t UndoRedoManager::transactionBytes(
    const Transaction& transaction) const {
    std::size_t total = sizeof(Transaction);
    for (const auto& batch : transaction.batches) {
        total += sizeof(EditBatch);
        for (const auto& record : batch) {
            addBytes(total, sizeof(EditRecord));
            addBytes(total, record.removedSnapshot.utf8.size());
            addBytes(total, record.insertedSnapshot.utf8.size());
            addBytes(total, (record.removedSnapshot.lineEndings.size() +
                             record.insertedSnapshot.lineEndings.size()) *
                                sizeof(LineEnding));
        }
    }
    addBytes(total, transaction.before.cursors.size() * sizeof(std::size_t));
    addBytes(total, transaction.after.cursors.size() * sizeof(std::size_t));
    addBytes(total, (transaction.before.selections.size() +
                     transaction.after.selections.size()) *
                        sizeof(Selection));
    return total;
}

void UndoRedoManager::applyBatch(const EditBatch& batch, bool inverse) {
    std::vector<const EditRecord*> records;
    records.reserve(batch.size());
    for (const auto& record : batch) records.push_back(&record);
    std::sort(records.begin(), records.end(), [inverse](const EditRecord* left,
                                                        const EditRecord* right) {
        return inverse ? left->offset < right->offset
                       : left->offset > right->offset;
    });
    for (const auto* record : records) {
        const auto& removed = inverse ? record->insertedSnapshot
                                      : record->removedSnapshot;
        const auto& inserted = inverse ? record->removedSnapshot
                                       : record->insertedSnapshot;
        buffer_.replace(record->offset, removed.utf8.size(), inserted);
        cursors_.onTextReplaced(record->offset, removed.utf8.size(),
                                inserted.utf8.size());
        selections_.onTextReplaced(record->offset, removed.utf8.size(),
                                   inserted.utf8.size());
    }
}

void UndoRedoManager::applyTransaction(const Transaction& transaction,
                                       bool inverse) {
    if (inverse) {
        for (auto batch = transaction.batches.rbegin();
             batch != transaction.batches.rend(); ++batch)
            applyBatch(*batch, true);
        restoreState(transaction.before);
    } else {
        for (const auto& batch : transaction.batches) applyBatch(batch, false);
        restoreState(transaction.after);
    }
}

UndoRedoManager::OperationKind UndoRedoManager::classifyTyping(
    const EditBatch& batch) const {
    if (batch.empty()) return OperationKind::other;
    for (const auto& record : batch)
        if (!singleScalar(record.insertedSnapshot.utf8) ||
            !record.removedSnapshot.utf8.empty())
            return OperationKind::other;
    return OperationKind::typing;
}

void UndoRedoManager::evictToLimit() noexcept {
    while (memoryUsage_ > memoryLimit_ && !undoHistory_.empty()) {
        memoryUsage_ -= undoHistory_.front().bytes;
        undoHistory_.erase(undoHistory_.begin());
    }
    while (memoryUsage_ > memoryLimit_ && !redoHistory_.empty()) {
        memoryUsage_ -= redoHistory_.front().bytes;
        redoHistory_.erase(redoHistory_.begin());
    }
}

} // namespace note::editor
