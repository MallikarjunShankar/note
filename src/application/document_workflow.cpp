#include "application/document_workflow.hpp"

#include <algorithm>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>
#include <exception>
#include <filesystem>
#include <memory>
#include <utility>

namespace note::application {
namespace {

WorkflowResult failed(std::string message,
                      std::optional<documents::DocumentId> id = std::nullopt) {
    return {OperationStatus::failed, id, std::move(message)};
}

} // namespace

DocumentWorkflow::~DocumentWorkflow() { shutdown(); }

DocumentWorkflow::OpenRequestId DocumentWorkflow::openAsync(
    const std::filesystem::path& path, OpenProgressCallback progress, OpenCompleteCallback complete) {
    if (path.empty() || shuttingDown_) return 0;
    std::error_code pathError;
    auto key = std::filesystem::weakly_canonical(path, pathError);
    if (pathError) {
        pathError.clear();
        key = std::filesystem::absolute(path, pathError);
        if (pathError) key = path;
    }
    key = key.lexically_normal();
    if (const auto it = pendingPaths_.find(key); it != pendingPaths_.end()) {
        if (const auto pending = pendingOpens_.find(it->second); pending != pendingOpens_.end())
            pending->second->observers.emplace_back(std::move(progress), std::move(complete));
        return it->second;
    }
    if (const auto existing = documents_.findByPath(path)) {
        ++selectionEpoch_;
        (void)documents_.setActiveDocument(*existing);
        const auto id = nextOpenRequestId_++;
        if (complete) QMetaObject::invokeMethod(this, [this, id, existing, complete = std::move(complete)]() mutable {
            if (!shuttingDown_) complete(id, {OperationStatus::alreadyOpen, existing, {}});
        }, Qt::QueuedConnection);
        return id;
    }

    const auto epochAtStart = ++selectionEpoch_;
    const auto requestId = nextOpenRequestId_++;
    auto pending = std::make_shared<PendingOpen>();
    pending->cancelled = std::make_shared<std::atomic_bool>(false);
    pending->observers.emplace_back(std::move(progress), std::move(complete));
    pendingOpens_.emplace(requestId, pending);
    pendingPaths_.emplace(key, requestId);
    QPointer<DocumentWorkflow> guard(this);
    const auto pool = [] {
        static auto* instance = [] { auto* value = new QThreadPool; value->setMaxThreadCount(2); return value; }();
        return instance;
    }();
    pool->start([guard, path, key, requestId, epochAtStart, pending]() mutable {
        documents::OpenOptions options;
        options.cancelled = pending->cancelled;
        options.progress = [guard, requestId, pending](documents::OpenProgress update) {
            if (!guard) return;
            QMetaObject::invokeMethod(guard, [guard, requestId, pending, update] {
                if (!guard || guard->shuttingDown_) return;
                const auto observers = pending->observers;
                for (const auto& observer : observers)
                    if (observer.first) observer.first(requestId, update);
            }, Qt::QueuedConnection);
        };
        auto opened = std::make_shared<documents::OpenFileResult>();
        if (!pending->cancelled->load()) {
            try { *opened = documents::FileService{}.open(path, options); }
            catch (const std::exception& error) { opened->error = error.what(); }
            catch (...) { opened->error = "An unexpected error occurred while opening the file."; }
        }
        if (!guard) return;
        QMetaObject::invokeMethod(guard, [guard, requestId, path, key, epochAtStart, pending, opened]() mutable {
            if (!guard) return;
            guard->pendingOpens_.erase(requestId);
            guard->pendingPaths_.erase(key);
            if (guard->shuttingDown_) return;
            WorkflowResult result;
            if (pending->cancelled->load() || opened->error == "Opening was cancelled.") {
                result = {OperationStatus::cancelled, std::nullopt, {}};
            } else if (!*opened) {
                result = failed(std::move(opened->error));
            } else {
                try {
                    if (const auto existing = guard->documents_.findByPath(path)) {
                        if (guard->selectionEpoch_ == epochAtStart)
                            (void)guard->documents_.setActiveDocument(*existing);
                        result = {OperationStatus::alreadyOpen, existing, {}};
                    } else {
                        const auto previouslyActive = guard->documents_.activeDocument();
                        auto& document = guard->documents_.registerOpened(std::move(opened->buffer), std::move(opened->metadata));
                        try { guard->tabs_.add(document.id()); }
                        catch (...) {
                            (void)guard->documents_.close(document.id(), true);
                            if (previouslyActive) (void)guard->documents_.setActiveDocument(*previouslyActive);
                            throw;
                        }
                        if (guard->selectionEpoch_ != epochAtStart && previouslyActive)
                            (void)guard->documents_.setActiveDocument(*previouslyActive);
                        result = {OperationStatus::success, document.id(), {}};
                    }
                } catch (const std::exception& error) { result = failed(error.what()); }
            }
            for (auto& observer : pending->observers)
                if (observer.second) observer.second(requestId, result);
            pending->observers.clear();
        }, Qt::QueuedConnection);
    });
    return requestId;
}

void DocumentWorkflow::cancelOpen(OpenRequestId id) noexcept {
    const auto it = pendingOpens_.find(id);
    if (it != pendingOpens_.end()) it->second->cancelled->store(true);
}

void DocumentWorkflow::shutdown() noexcept {
    if (shuttingDown_) return;
    shuttingDown_ = true;
    for (auto& [id, request] : pendingOpens_) { (void)id; request->cancelled->store(true); }
    pendingOpens_.clear();
    pendingPaths_.clear();
}

documents::Document& DocumentWorkflow::createNew() {
    ++selectionEpoch_;
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
        ++selectionEpoch_;
        (void)documents_.setActiveDocument(*existing);
        return {OperationStatus::alreadyOpen, existing, {}};
    }

    ++selectionEpoch_;
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
    ++selectionEpoch_;
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
    if (wasActive) ++selectionEpoch_;
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
