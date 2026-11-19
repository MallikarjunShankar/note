#include "editor/selection_manager.hpp"

#include "editor/text_buffer.hpp"

#include <limits>
#include <stdexcept>

namespace note::editor {

SelectionManager::SelectionManager(const TextBuffer& buffer)
    : buffer_(buffer), selections_{{0, 0}} {}

std::size_t SelectionManager::selectionCount() const noexcept {
    return selections_.size();
}

const Selection& SelectionManager::selection(std::size_t index) const {
    if (index >= selections_.size()) throw std::out_of_range("selection index");
    return selections_[index];
}

Selection& SelectionManager::selectionAt(std::size_t index) {
    if (index >= selections_.size()) throw std::out_of_range("selection index");
    return selections_[index];
}

SelectionRange SelectionManager::normalizedRange(std::size_t index) const {
    const auto& value = selection(index);
    if (value.anchor <= value.active) return {value.anchor, value.active};
    return {value.active, value.anchor};
}

bool SelectionManager::hasSelection(std::size_t index) const {
    const auto range = normalizedRange(index);
    return range.begin != range.end;
}

std::size_t SelectionManager::clampOffset(std::size_t offset) const {
    if (offset >= buffer_.sizeBytes()) return buffer_.sizeBytes();
    while (offset > 0 && !buffer_.isScalarBoundary(offset)) --offset;
    return offset;
}

void SelectionManager::setSelection(std::size_t index, std::size_t anchor,
                                    std::size_t active) {
    auto& value = selectionAt(index);
    value.anchor = clampOffset(anchor);
    value.active = clampOffset(active);
}

std::size_t SelectionManager::addSelection(std::size_t anchor, std::size_t active) {
    selections_.push_back({clampOffset(anchor), clampOffset(active)});
    return selections_.size() - 1;
}

bool SelectionManager::removeSelection(std::size_t index) {
    if (index >= selections_.size()) return false;
    selections_.erase(selections_.begin() +
                      static_cast<std::vector<Selection>::difference_type>(index));
    return true;
}

void SelectionManager::setAnchor(std::size_t index, std::size_t offset) {
    selectionAt(index).anchor = clampOffset(offset);
}

void SelectionManager::setActive(std::size_t index, std::size_t offset) {
    selectionAt(index).active = clampOffset(offset);
}

void SelectionManager::clearSelection(std::size_t index) {
    auto& value = selectionAt(index);
    value.anchor = value.active;
}

void SelectionManager::clearSelections() noexcept {
    selections_.clear();
}

void SelectionManager::syncToBuffer() {
    for (auto& value : selections_) {
        value.anchor = clampOffset(value.anchor);
        value.active = clampOffset(value.active);
    }
}

void SelectionManager::onTextReplaced(std::size_t offset, std::size_t removedBytes,
                                      std::size_t insertedBytes) {
    const auto currentSize = buffer_.sizeBytes();
    if (removedBytes > std::numeric_limits<std::size_t>::max() - currentSize ||
        insertedBytes > currentSize + removedBytes) {
        throw std::invalid_argument("selection edit transformation has invalid sizes");
    }
    const auto oldSize = currentSize + removedBytes - insertedBytes;
    if (offset > oldSize || removedBytes > oldSize - offset) {
        throw std::invalid_argument("selection edit transformation has invalid range");
    }

    const auto end = offset + removedBytes;
    auto transform = [&](std::size_t endpoint) {
        if (removedBytes == 0) {
            if (endpoint >= offset) endpoint += insertedBytes;
        } else if (endpoint >= end) {
            endpoint = endpoint - removedBytes + insertedBytes;
        } else if (endpoint > offset) {
            endpoint = offset + insertedBytes;
        }
        return clampOffset(endpoint);
    };
    for (auto& value : selections_) {
        value.anchor = transform(value.anchor);
        value.active = transform(value.active);
    }
}

} // namespace note::editor
