#pragma once

#include "editor/text_buffer.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace note::documents {

using DocumentId = std::uint64_t;

enum class FileEncoding : std::uint8_t { utf8, utf8Bom, utf16LittleEndian, utf16BigEndian };

struct FileMetadata {
    std::uintmax_t byteSize{};
    std::int64_t modifiedTimeMilliseconds{};
};

struct DocumentMetadata {
    std::optional<std::filesystem::path> path;
    FileEncoding encoding{FileEncoding::utf8};
    editor::LineEnding preferredLineEnding{editor::LineEnding::lf};
    bool mixedLineEndings{};
    std::optional<FileMetadata> file;
};

class DocumentManager;
class FileService;

// The document owns metadata only. Its manager owns the referenced buffer and
// the document object's lifetime; the reference is invalid after manager close.
class Document final {
public:
    [[nodiscard]] DocumentId id() const noexcept { return id_; }
    [[nodiscard]] editor::TextBuffer& buffer() noexcept { return buffer_; }
    [[nodiscard]] const editor::TextBuffer& buffer() const noexcept { return buffer_; }
    [[nodiscard]] const DocumentMetadata& metadata() const noexcept { return metadata_; }
    [[nodiscard]] bool modified() const noexcept { return savedRevision_ != buffer_.revision(); }

private:
    friend class DocumentManager;
    friend class FileService;
    Document(DocumentId id, editor::TextBuffer& buffer, DocumentMetadata metadata);
    bool recordSuccessfulSave(std::uint64_t savedRevision, DocumentMetadata metadata);

    DocumentId id_{};
    editor::TextBuffer& buffer_;
    DocumentMetadata metadata_;
    std::uint64_t savedRevision_{};
};

} // namespace note::documents
