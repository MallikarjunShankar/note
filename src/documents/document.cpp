#include "documents/document.hpp"

#include <utility>

namespace note::documents {

Document::Document(DocumentId id, editor::TextBuffer& buffer, DocumentMetadata metadata)
    : id_(id), buffer_(buffer), metadata_(std::move(metadata)), savedRevision_(buffer.revision()) {}

bool Document::recordSuccessfulSave(std::uint64_t savedRevision, DocumentMetadata metadata) {
    metadata_ = std::move(metadata);
    if (buffer_.revision() != savedRevision) return false;
    savedRevision_ = savedRevision;
    return true;
}

} // namespace note::documents
