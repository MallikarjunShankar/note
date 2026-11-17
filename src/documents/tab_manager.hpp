#pragma once

#include "documents/document.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace note::documents {

// Owns tab ordering only. Document lifetime and active identity remain with
// DocumentManager; this type stores no document contents or metadata.
class TabManager final {
public:
    void add(DocumentId id);
    bool remove(DocumentId id) noexcept;
    [[nodiscard]] bool contains(DocumentId id) const noexcept;
    [[nodiscard]] std::optional<std::size_t> indexOf(DocumentId id) const noexcept;
    [[nodiscard]] std::optional<DocumentId> at(std::size_t index) const noexcept;
    [[nodiscard]] const std::vector<DocumentId>& order() const noexcept { return order_; }
    [[nodiscard]] std::size_t size() const noexcept { return order_.size(); }

private:
    std::vector<DocumentId> order_;
};

} // namespace note::documents
