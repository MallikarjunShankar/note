#include "documents/tab_manager.hpp"

#include <algorithm>
#include <stdexcept>

namespace note::documents {

void TabManager::add(DocumentId id) {
    if (contains(id)) throw std::invalid_argument("document already has a tab");
    order_.push_back(id);
}

bool TabManager::remove(DocumentId id) noexcept {
    const auto it = std::find(order_.begin(), order_.end(), id);
    if (it == order_.end()) return false;
    order_.erase(it);
    return true;
}

bool TabManager::contains(DocumentId id) const noexcept {
    return std::find(order_.begin(), order_.end(), id) != order_.end();
}

std::optional<std::size_t> TabManager::indexOf(DocumentId id) const noexcept {
    const auto it = std::find(order_.begin(), order_.end(), id);
    if (it == order_.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(order_.begin(), it));
}

std::optional<DocumentId> TabManager::at(std::size_t index) const noexcept {
    if (index >= order_.size()) return std::nullopt;
    return order_[index];
}

} // namespace note::documents
