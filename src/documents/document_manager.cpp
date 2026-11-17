#include "documents/document_manager.hpp"

#include <stdexcept>
#include <system_error>
#include <utility>

namespace note::documents {
namespace {

std::filesystem::path normalizedPath(const std::filesystem::path& path) {
    std::error_code error;
    auto result = std::filesystem::weakly_canonical(path, error);
    if (!error) return result;
    error.clear();
    result = std::filesystem::absolute(path, error);
    return (error ? path : result).lexically_normal();
}

} // namespace

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
    return findByPath(path).has_value();
}

std::optional<DocumentId> DocumentManager::findByPath(
    const std::filesystem::path& path, std::optional<DocumentId> except) const {
    const auto normalizedCandidate = normalizedPath(path);
    for (const auto& [id, entry] : documents_) {
        if (except == id) continue;
        const auto& existingPath = entry.document->metadata().path;
        if (!existingPath) continue;
        const auto normalizedExisting = normalizedPath(*existingPath);
        if (normalizedCandidate == normalizedExisting) return id;
        std::error_code equivalentError;
        if (std::filesystem::equivalent(path, *existingPath, equivalentError) && !equivalentError) return id;
    }
    return std::nullopt;
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

bool DocumentManager::replaceOpenedContents(DocumentId id,
                                             std::unique_ptr<editor::TextBuffer> buffer,
                                             DocumentMetadata metadata) {
    const auto it = documents_.find(id);
    if (it == documents_.end() || !buffer || !metadata.path) return false;
    *it->second.buffer = std::move(*buffer);
    const auto revision = it->second.buffer->revision();
    (void)it->second.document->recordSuccessfulSave(revision, std::move(metadata));
    return true;
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
bool DocumentManager::setExternalFileState(DocumentId id, ExternalFileState state) noexcept {
    const auto it = documents_.find(id);
    if (it == documents_.end()) return false;
    it->second.document->recordExternalFileState(state);
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
