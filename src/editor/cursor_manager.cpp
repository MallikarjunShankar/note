#include "editor/cursor_manager.hpp"

#include "editor/text_buffer.hpp"

#include <algorithm>
#include <stdexcept>

namespace note::editor {

CursorManager::CursorManager(const TextBuffer& buffer)
    : buffer_(buffer), primary_(CursorState{0, 0}) {
    syncToBuffer();
}

std::size_t CursorManager::primaryOffset() const noexcept {
    return primary_.offset;
}

std::size_t CursorManager::primaryCursor() const noexcept {
    return primary_.offset;
}

CursorLocation CursorManager::primaryLocation() const {
    return lineAndColumn(primary_.offset);
}

void CursorManager::setPrimaryCursor(std::size_t offset) {
    setCursor(offset);
}

void CursorManager::setPrimaryPosition(std::size_t offset) {
    setCursor(offset);
}

void CursorManager::setCursor(std::size_t offset) {
    primary_.offset = clampOffset(offset);
    primary_.preferredColumn = lineAndColumn(primary_.offset).column;
}

void CursorManager::moveToOffset(std::size_t offset) {
    setCursor(offset);
}

std::size_t CursorManager::cursorCount() const noexcept {
    return 1 + secondary_.size();
}

std::size_t CursorManager::secondaryCursorCount() const noexcept {
    return secondary_.size();
}

const std::vector<std::size_t>& CursorManager::secondaryCursorOffsets() const noexcept {
    return secondaryOffsets_;
}

std::size_t CursorManager::secondaryCursorOffset(std::size_t index) const {
    if (index >= secondary_.size()) throw std::out_of_range("secondary cursor index");
    return secondary_[index].offset;
}

bool CursorManager::addSecondaryCursor(std::size_t offset) {
    const auto clamped = clampOffset(offset);
    if (clamped == primary_.offset) return false;
    if (std::any_of(secondary_.begin(), secondary_.end(),
                    [clamped](const CursorState& cursor) { return cursor.offset == clamped; }))
        return false;
    const auto location = lineAndColumn(clamped);
    secondary_.push_back({clamped, location.column});
    secondaryOffsets_.push_back(clamped);
    return true;
}

void CursorManager::setSecondaryCursor(std::size_t index, std::size_t offset) {
    if (index >= secondary_.size()) throw std::out_of_range("secondary cursor index");
    secondary_[index].offset = clampOffset(offset);
    secondary_[index].preferredColumn = lineAndColumn(secondary_[index].offset).column;
    syncToBuffer();
}

bool CursorManager::removeSecondaryCursor(std::size_t index) {
    if (index >= secondary_.size()) return false;
    secondary_.erase(secondary_.begin() +
                     static_cast<std::vector<CursorState>::difference_type>(index));
    secondaryOffsets_.erase(secondaryOffsets_.begin() +
                            static_cast<std::vector<std::size_t>::difference_type>(index));
    return true;
}

void CursorManager::clearSecondaryCursors() {
    secondary_.clear();
    secondaryOffsets_.clear();
}

std::size_t CursorManager::clampOffset(std::size_t offset) const {
    if (offset >= buffer_.sizeBytes()) return buffer_.sizeBytes();
    return clampWithinScalar(offset);
}

std::size_t CursorManager::clampWithinScalar(std::size_t offset) const {
    if (offset == 0) return 0;
    if (buffer_.isScalarBoundary(offset)) return offset;
    while (offset > 0 && !buffer_.isScalarBoundary(offset))
        --offset;
    return offset;
}

std::size_t CursorManager::moveOffsetLeft(std::size_t offset) const {
    if (offset == 0) return 0;
    --offset;
    while (offset > 0 && !buffer_.isScalarBoundary(offset))
        --offset;
    return offset;
}

std::size_t CursorManager::moveOffsetRight(std::size_t offset) const {
    if (offset >= buffer_.sizeBytes()) return buffer_.sizeBytes();
    ++offset;
    while (offset < buffer_.sizeBytes() && !buffer_.isScalarBoundary(offset))
        ++offset;
    return clampOffset(offset);
}

std::size_t CursorManager::lineCount() const {
    return buffer_.lineCount();
}

CursorLocation CursorManager::lineAndColumn(std::size_t offset) const {
    const auto clamped = clampOffset(offset);
    std::size_t low = 0;
    std::size_t high = lineCount();
    std::size_t line = high - 1;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        const auto range = buffer_.lineRange(middle);
        if (clamped < range.begin) {
            high = middle;
        } else if (clamped > range.end) {
            low = middle + 1;
        } else {
            line = middle;
            const auto begin = range.begin;
            return {line, clamped - begin};
        }
    }
    line = std::min(low, lineCount() - 1);
    const auto range = buffer_.lineRange(line);
    return {line, clamped >= range.begin ? clamped - range.begin : 0};
}

std::size_t CursorManager::clampLineColumn(std::size_t line, std::size_t column) const {
    if (line >= lineCount()) line = lineCount() - 1;
    const auto range = buffer_.lineRange(line);
    std::size_t target = range.begin + std::min(column, range.end - range.begin);
    target = clampOffset(target);
    if (target < range.begin) target = range.begin;
    if (target > range.end) target = range.end;
    return target;
}

void CursorManager::moveLeft() {
    primary_.offset = moveOffsetLeft(primary_.offset);
    primary_.preferredColumn = lineAndColumn(primary_.offset).column;
    for (auto& cursor : secondary_) {
        cursor.offset = moveOffsetLeft(cursor.offset);
        cursor.preferredColumn = lineAndColumn(cursor.offset).column;
    }
    syncToBuffer();
}

void CursorManager::moveRight() {
    primary_.offset = moveOffsetRight(primary_.offset);
    primary_.preferredColumn = lineAndColumn(primary_.offset).column;
    for (auto& cursor : secondary_) {
        cursor.offset = moveOffsetRight(cursor.offset);
        cursor.preferredColumn = lineAndColumn(cursor.offset).column;
    }
    syncToBuffer();
}

void CursorManager::moveUp() {
    syncState(primary_);
    const auto primaryColumn = primary_.preferredColumn;
    const auto primaryLocation = lineAndColumn(primary_.offset);
    if (primaryLocation.line > 0)
        primary_.offset = clampLineColumn(primaryLocation.line - 1, primaryColumn);
    primary_.preferredColumn = primaryColumn;
    for (auto& cursor : secondary_) {
        syncState(cursor);
        const auto column = cursor.preferredColumn;
        const auto location = lineAndColumn(cursor.offset);
        if (location.line > 0)
            cursor.offset = clampLineColumn(location.line - 1, column);
        cursor.preferredColumn = column;
    }
    syncToBuffer();
}

void CursorManager::moveDown() {
    syncState(primary_);
    const auto primaryColumn = primary_.preferredColumn;
    const auto primaryLocation = lineAndColumn(primary_.offset);
    if (primaryLocation.line + 1 < lineCount())
        primary_.offset = clampLineColumn(primaryLocation.line + 1, primaryColumn);
    primary_.preferredColumn = primaryColumn;
    for (auto& cursor : secondary_) {
        syncState(cursor);
        const auto column = cursor.preferredColumn;
        const auto location = lineAndColumn(cursor.offset);
        if (location.line + 1 < lineCount())
            cursor.offset = clampLineColumn(location.line + 1, column);
        cursor.preferredColumn = column;
    }
    syncToBuffer();
}

void CursorManager::moveToDocumentStart() {
    setCursor(0);
}

void CursorManager::moveToDocumentEnd() {
    setCursor(buffer_.sizeBytes());
}

void CursorManager::moveToLineColumn(std::size_t line, std::size_t column) {
    primary_.offset = clampLineColumn(line, column);
    primary_.preferredColumn = lineAndColumn(primary_.offset).column;
}

void CursorManager::syncState(CursorState& cursor) {
    cursor.offset = clampOffset(cursor.offset);
}

void CursorManager::syncToBuffer() {
    syncState(primary_);
    secondaryOffsets_.clear();
    secondaryOffsets_.reserve(secondary_.size());
    for (auto& cursor : secondary_) {
        syncState(cursor);
        secondaryOffsets_.push_back(cursor.offset);
    }
}

void CursorManager::onTextReplaced(std::size_t offset, std::size_t removedBytes,
                                   std::size_t insertedBytes) {
    if (offset > buffer_.sizeBytes() && removedBytes == 0) offset = buffer_.sizeBytes();
    const auto end = offset + removedBytes;
    auto adjust = [&](CursorState& cursor) {
        if (removedBytes == 0) {
            if (cursor.offset >= offset) cursor.offset += insertedBytes;
        } else if (cursor.offset >= end) {
            cursor.offset = cursor.offset - removedBytes + insertedBytes;
        } else if (cursor.offset > offset) {
            cursor.offset = offset + insertedBytes;
        }
    };
    adjust(primary_);
    for (auto& cursor : secondary_) adjust(cursor);
    syncToBuffer();
    primary_.preferredColumn = lineAndColumn(primary_.offset).column;
    for (auto& cursor : secondary_)
        cursor.preferredColumn = lineAndColumn(cursor.offset).column;
}

} // namespace note::editor
