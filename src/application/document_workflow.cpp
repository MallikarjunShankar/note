#include "application/document_workflow.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace note::application {
namespace {

WorkflowResult failed(std::string message,
                      std::optional<documents::DocumentId> id = std::nullopt) {
    return {OperationStatus::failed, id, std::move(message)};
}

} // namespace

documents::Document& DocumentWorkflow::createNew() {
    const auto previousActive = documents_.activeDocument();
    auto& document = documents_.createNew();
    try {
        tabs_.add(document.id());
    } catch (...) {
        (void)documents_.close(document.id(), true);
        if (previousActive) (void)documents_.setActiveDocument(*previousActive);
        throw;
    }
    return document;
}

WorkflowResult DocumentWorkflow::open(const std::filesystem::path& path) {
    if (path.empty()) return {OperationStatus::cancelled, std::nullopt, {}};
    if (const auto existing = documents_.findByPath(path)) {
        (void)documents_.setActiveDocument(*existing);
        return {OperationStatus::alreadyOpen, existing, {}};
    }

    const auto previousActive = documents_.activeDocument();
    auto opened = files_.open(path);
    if (!opened) return failed(std::move(opened.error));

    try {
        auto& document = documents_.registerOpened(std::move(opened.buffer), std::move(opened.metadata));
        try {
            tabs_.add(document.id());
        } catch (...) {
            (void)documents_.close(document.id(), true);
            if (previousActive) (void)documents_.setActiveDocument(*previousActive);
            throw;
        }
        return {OperationStatus::success, document.id(), {}};
    } catch (const std::exception& error) {
        return failed(error.what());
    }
}

WorkflowResult DocumentWorkflow::activate(documents::DocumentId id) noexcept {
    if (!tabs_.contains(id) || !documents_.setActiveDocument(id)) {
        return {OperationStatus::notFound, id, {}};
    }
    return {OperationStatus::success, id, {}};
}

WorkflowResult DocumentWorkflow::save(documents::DocumentId id) {
    auto* document = documents_.find(id);
    if (!document) return {OperationStatus::notFound, id, {}};
    if (!document->metadata().path) return {OperationStatus::needsSavePath, id, {}};
    const auto result = files_.save(*document);
    if (!result) return failed(result.error, id);
    return {OperationStatus::success, id, {}};
}

WorkflowResult DocumentWorkflow::saveAs(documents::DocumentId id,
                                        const std::filesystem::path& path) {
    auto* document = documents_.find(id);
    if (!document) return {OperationStatus::notFound, id, {}};
    if (path.empty()) return {OperationStatus::cancelled, id, {}};
    if (const auto collision = documents_.findByPath(path, id)) {
        return failed("That file is already open in another tab.", id);
    }
    const auto result = files_.saveAs(*document, path);
    if (!result) return failed(result.error, id);
    return {OperationStatus::success, id, {}};
}

WorkflowResult DocumentWorkflow::close(documents::DocumentId id, CloseChoice choice,
                                       std::optional<std::filesystem::path> saveAsPath) {
    auto* document = documents_.find(id);
    if (!document || !tabs_.contains(id)) return {OperationStatus::notFound, id, {}};

    if (document->modified()) {
        if (choice == CloseChoice::cancel) return {OperationStatus::cancelled, id, {}};
        if (choice == CloseChoice::save) {
            const auto saved = document->metadata().path
                ? save(id)
                : (saveAsPath ? saveAs(id, *saveAsPath)
                              : WorkflowResult{OperationStatus::needsSavePath, id, {}});
            if (!saved) return saved;
        }
    }

    const auto oldIndex = tabs_.indexOf(id);
    const auto wasActive = documents_.activeDocument() == id;
    const auto closeResult = documents_.close(id, choice == CloseChoice::discard);
    if (closeResult == documents::CloseResult::confirmationRequired) {
        return failed("Document remains modified; it was not closed.", id);
    }
    if (closeResult != documents::CloseResult::closed) return {OperationStatus::notFound, id, {}};

    (void)tabs_.remove(id);
    if (wasActive && tabs_.size() > 0) {
        const auto nextIndex = std::min(*oldIndex, tabs_.size() - 1);
        const auto next = tabs_.at(nextIndex);
        if (next) (void)documents_.setActiveDocument(*next);
    }
    return {OperationStatus::success, id, {}};
}

documents::Document* DocumentWorkflow::activeDocument() noexcept {
    const auto id = documents_.activeDocument();
    return id ? documents_.find(*id) : nullptr;
}

documents::Document* DocumentWorkflow::findDocument(documents::DocumentId id) noexcept {
    return documents_.find(id);
}

const documents::Document* DocumentWorkflow::findDocument(documents::DocumentId id) const noexcept {
    return documents_.find(id);
}

const documents::Document* DocumentWorkflow::activeDocument() const noexcept {
    const auto id = documents_.activeDocument();
    return id ? documents_.find(*id) : nullptr;
}

} // namespace note::application
