#pragma once

#include "editor/text_buffer.hpp"

#include <cstdint>
#include <array>
#include <filesystem>
#include <optional>

namespace note::documents {

using DocumentId = std::uint64_t;

enum class FileEncoding : std::uint8_t { utf8, utf8Bom, utf16LittleEndian, utf16BigEndian };

struct FileMetadata {
    std::uintmax_t byteSize{};
    std::int64_t modifiedTimeMilliseconds{};
    std::array<std::uint8_t, 32> sha256{};
    bool hasFingerprint{};
};

enum class ExternalFileState : std::uint8_t { current, changed, missing };

struct DocumentMetadata {
    std::optional<std::filesystem::path> path;
    FileEncoding encoding{FileEncoding::utf8};
    editor::LineEnding preferredLineEnding{editor::LineEnding::lf};
    bool mixedLineEndings{};
    std::optional<FileMetadata> file;
    ExternalFileState externalFileState{ExternalFileState::current};
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
    [[nodiscard]] std::uint64_t contentGeneration() const noexcept { return contentGeneration_; }

private:
    friend class DocumentManager;
    friend class FileService;
    Document(DocumentId id, editor::TextBuffer& buffer, DocumentMetadata metadata);
    bool recordSuccessfulSave(std::uint64_t savedRevision, DocumentMetadata metadata);
    void recordContentReplacement() noexcept { ++contentGeneration_; }
    void recordExternalFileState(ExternalFileState state) noexcept { metadata_.externalFileState = state; }

    DocumentId id_{};
    editor::TextBuffer& buffer_;
    DocumentMetadata metadata_;
    std::uint64_t savedRevision_{};
    std::uint64_t contentGeneration_{};
};

} // namespace note::documents
