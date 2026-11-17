#pragma once

#include "documents/document.hpp"

#include <map>
#include <memory>
#include <optional>

namespace note::documents {

enum class CloseResult { closed, notFound, confirmationRequired };

// Owns open Document and TextBuffer instances, but does not own tab order.
class DocumentManager final {
public:
    Document& createNew();
    Document& registerOpened(std::unique_ptr<editor::TextBuffer> buffer, DocumentMetadata metadata);
    [[nodiscard]] Document* find(DocumentId id) noexcept;
    [[nodiscard]] const Document* find(DocumentId id) const noexcept;
    [[nodiscard]] std::optional<DocumentId> findByPath(
        const std::filesystem::path& path,
        std::optional<DocumentId> except = std::nullopt) const;
    [[nodiscard]] std::size_t size() const noexcept { return documents_.size(); }
    [[nodiscard]] std::optional<DocumentId> activeDocument() const noexcept { return active_; }
    bool setActiveDocument(DocumentId id) noexcept;
    CloseResult close(DocumentId id, bool discardModified = false);

private:
    struct Entry {
        std::unique_ptr<editor::TextBuffer> buffer;
        std::unique_ptr<Document> document;
    };
    [[nodiscard]] bool pathAlreadyOpen(const std::filesystem::path& path) const;
    DocumentId nextId_{1};
    std::map<DocumentId, Entry> documents_;
    std::optional<DocumentId> active_;
};

} // namespace note::documents
