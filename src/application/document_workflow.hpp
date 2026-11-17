#pragma once

#include "documents/document_manager.hpp"
#include "documents/file_service.hpp"
#include "documents/tab_manager.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace note::application {

enum class OperationStatus { success, alreadyOpen, cancelled, needsSavePath, notFound, failed };

struct WorkflowResult {
    OperationStatus status{OperationStatus::failed};
    std::optional<documents::DocumentId> documentId;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept {
        return status == OperationStatus::success || status == OperationStatus::alreadyOpen;
    }
};

enum class CloseChoice { save, discard, cancel };

// Application-level orchestration only. The manager owns documents and active
// identity; TabManager owns order; FileService owns persistence.
class DocumentWorkflow final {
public:
    [[nodiscard]] documents::Document& createNew();
    [[nodiscard]] WorkflowResult open(const std::filesystem::path& path);
    [[nodiscard]] WorkflowResult activate(documents::DocumentId id) noexcept;
    [[nodiscard]] WorkflowResult save(documents::DocumentId id);
    [[nodiscard]] WorkflowResult saveAs(documents::DocumentId id,
                                        const std::filesystem::path& path);
    [[nodiscard]] WorkflowResult close(documents::DocumentId id, CloseChoice choice,
                                       std::optional<std::filesystem::path> saveAsPath = std::nullopt);

    [[nodiscard]] documents::Document* findDocument(documents::DocumentId id) noexcept;
    [[nodiscard]] const documents::Document* findDocument(documents::DocumentId id) const noexcept;
    [[nodiscard]] std::size_t documentCount() const noexcept { return documents_.size(); }
    [[nodiscard]] const documents::TabManager& tabs() const noexcept { return tabs_; }
    [[nodiscard]] std::optional<documents::DocumentId> activeDocumentId() const noexcept {
        return documents_.activeDocument();
    }
    [[nodiscard]] documents::Document* activeDocument() noexcept;
    [[nodiscard]] const documents::Document* activeDocument() const noexcept;

private:
    documents::DocumentManager documents_;
    documents::TabManager tabs_;
    documents::FileService files_;
};

} // namespace note::application
