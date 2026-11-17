#include "documents/document_manager.hpp"

#include <stdexcept>
#include <system_error>
#include <utility>

namespace note::documents {

Document& DocumentManager::createNew() {
    auto buffer = std::make_unique<editor::TextBuffer>();
    const auto id = nextId_++;
    Entry entry;
    entry.buffer = std::move(buffer);
    entry.document = std::unique_ptr<Document>(new Document(id, *entry.buffer, {}));
    auto [it, inserted] = documents_.emplace(id, std::move(entry));
    if (!inserted) throw std::logic_error("document identity collision");
    active_ = id;
    return *it->second.document;
}

bool DocumentManager::pathAlreadyOpen(const std::filesystem::path& path) const {
    std::error_code candidateError;
    const auto candidate = std::filesystem::weakly_canonical(path, candidateError);
    const auto normalizedCandidate = candidateError ? std::filesystem::absolute(path).lexically_normal() : candidate;
    for (const auto& [id, entry] : documents_) {
        (void)id;
        const auto& existingPath = entry.document->metadata().path;
        if (!existingPath) continue;
        std::error_code existingError;
        const auto existing = std::filesystem::weakly_canonical(*existingPath, existingError);
        const auto normalizedExisting = existingError ? std::filesystem::absolute(*existingPath).lexically_normal() : existing;
        if (normalizedCandidate == normalizedExisting) return true;
        std::error_code equivalentError;
        if (std::filesystem::equivalent(path, *existingPath, equivalentError) && !equivalentError) return true;
    }
    return false;
}

Document& DocumentManager::registerOpened(std::unique_ptr<editor::TextBuffer> buffer,
                                          DocumentMetadata metadata) {
    if (!buffer) throw std::invalid_argument("opened document requires a text buffer");
    if (!metadata.path) throw std::invalid_argument("opened document requires a file path");
    if (pathAlreadyOpen(*metadata.path)) throw std::invalid_argument("file is already registered");
    const auto id = nextId_++;
    Entry entry;
    entry.buffer = std::move(buffer);
    entry.document = std::unique_ptr<Document>(new Document(id, *entry.buffer, std::move(metadata)));
    auto [it, inserted] = documents_.emplace(id, std::move(entry));
    if (!inserted) throw std::logic_error("document identity collision");
    active_ = id;
    return *it->second.document;
}

Document* DocumentManager::find(DocumentId id) noexcept {
    const auto it = documents_.find(id);
    return it == documents_.end() ? nullptr : it->second.document.get();
}
const Document* DocumentManager::find(DocumentId id) const noexcept {
    const auto it = documents_.find(id);
    return it == documents_.end() ? nullptr : it->second.document.get();
}
bool DocumentManager::setActiveDocument(DocumentId id) noexcept {
    if (!documents_.contains(id)) return false;
    active_ = id;
    return true;
}
CloseResult DocumentManager::close(DocumentId id, bool discardModified) {
    const auto it = documents_.find(id);
    if (it == documents_.end()) return CloseResult::notFound;
    if (it->second.document->modified() && !discardModified) return CloseResult::confirmationRequired;
    documents_.erase(it);
    if (active_ == id) active_.reset();
    return CloseResult::closed;
}

} // namespace note::documents
