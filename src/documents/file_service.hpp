#pragma once

#include "documents/document.hpp"

#include <memory>
#include <atomic>
#include <functional>
#include <string>

namespace note::documents {

struct OpenFileResult {
    std::unique_ptr<editor::TextBuffer> buffer;
    DocumentMetadata metadata;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return buffer != nullptr; }
};

struct FileOperationResult {
    bool success{};
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

struct OpenProgress {
    std::uintmax_t bytesRead{};
    std::uintmax_t totalBytes{};
    bool buildingIndex{};
};

struct OpenOptions {
    std::shared_ptr<std::atomic_bool> cancelled;
    std::function<void(OpenProgress)> progress;
    std::size_t chunkBytes{256 * 1024};
};

// Persistence facade. All filesystem access is confined to this component.
class FileService final {
public:
    [[nodiscard]] OpenFileResult open(const std::filesystem::path& path) const;
    [[nodiscard]] OpenFileResult open(const std::filesystem::path& path, const OpenOptions& options) const;
    [[nodiscard]] FileOperationResult save(Document& document) const;
    [[nodiscard]] FileOperationResult saveAs(Document& document, const std::filesystem::path& path) const;
};

} // namespace note::documents
