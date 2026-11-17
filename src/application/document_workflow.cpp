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

QThreadPool& backgroundPool() {
    static auto* pool = [] {
        auto* value = new QThreadPool;
        value->setMaxThreadCount(2);
        return value;
    }();
    return *pool;
}

} // namespace

DocumentWorkflow::DocumentWorkflow()
    : fileMonitor_([this](documents::DocumentId id) { scheduleExternalInspection(id); }) {}

DocumentWorkflow::~DocumentWorkflow() { shutdown(); }

DocumentWorkflow::OpenRequestId DocumentWorkflow::openAsync(
    const std::filesystem::path& path, OpenProgressCallback progress, OpenCompleteCallback complete) {
    return startAsyncLoad(path, std::nullopt, false, std::nullopt,
                          std::move(progress), std::move(complete));
}

DocumentWorkflow::OpenRequestId DocumentWorkflow::reloadAsync(
    documents::DocumentId id, bool confirmDiscardLocalChanges,
    OpenProgressCallback progress, OpenCompleteCallback complete) {
    const auto* document = documents_.find(id);
    if (!document || !document->metadata().path ||
        (document->modified() && !confirmDiscardLocalChanges)) return 0;
    const auto revision = document->buffer().revision();
    return startAsyncLoad(*document->metadata().path, id, confirmDiscardLocalChanges, revision,
                          std::move(progress), std::move(complete));
}

DocumentWorkflow::OpenRequestId DocumentWorkflow::startAsyncLoad(
    const std::filesystem::path& path, std::optional<documents::DocumentId> reloadDocument,
    bool confirmDiscardLocalChanges, std::optional<std::uint64_t> reloadRevision,
    OpenProgressCallback progress, OpenCompleteCallback complete) {
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
        if (const auto pending = pendingOpens_.find(it->second); pending != pendingOpens_.end()) {
            if (pending->second->reloadDocument != reloadDocument) return 0;
            pending->second->observers.emplace_back(std::move(progress), std::move(complete));
        }
        return it->second;
    }
    if (!reloadDocument) if (const auto existing = documents_.findByPath(path)) {
        ++selectionEpoch_;
        (void)documents_.setActiveDocument(*existing);
        const auto id = nextOpenRequestId_++;
        if (complete) QMetaObject::invokeMethod(this, [this, id, existing, complete = std::move(complete)]() mutable {
            if (!shuttingDown_) complete(id, {OperationStatus::alreadyOpen, existing, {}});
        }, Qt::QueuedConnection);
        return id;
    }

    const auto epochAtStart = reloadDocument ? selectionEpoch_ : ++selectionEpoch_;
    const auto requestId = nextOpenRequestId_++;
    auto pending = std::make_shared<PendingOpen>();
    pending->cancelled = std::make_shared<std::atomic_bool>(false);
    pending->observers.emplace_back(std::move(progress), std::move(complete));
    pending->reloadDocument = reloadDocument;
    pending->confirmDiscardLocalChanges = confirmDiscardLocalChanges;
    pending->reloadRevision = reloadRevision;
    pendingOpens_.emplace(requestId, pending);
    pendingPaths_.emplace(key, requestId);
    QPointer<DocumentWorkflow> guard(this);
    backgroundPool().start([guard, path, key, requestId, epochAtStart, pending]() mutable {
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
            } else if (pending->reloadDocument) {
                auto* document = guard->documents_.find(*pending->reloadDocument);
                if (!document) result = {OperationStatus::notFound, pending->reloadDocument, {}};
                else if (pending->reloadRevision && document->buffer().revision() != *pending->reloadRevision)
                    result = {OperationStatus::conflict, pending->reloadDocument,
                              "The document changed again while reloading. Its current contents were preserved."};
                else if (document->modified() && !pending->confirmDiscardLocalChanges)
                    result = {OperationStatus::conflict, pending->reloadDocument,
                              "The document changed locally while reloading. Its edits were preserved."};
                else if (guard->documents_.replaceOpenedContents(*pending->reloadDocument,
                          std::move(opened->buffer), std::move(opened->metadata)))
                {
                    guard->fileMonitor_.monitor(*pending->reloadDocument, *document->metadata().path);
                    guard->scheduleExternalInspection(*pending->reloadDocument);
                    result = {OperationStatus::success, pending->reloadDocument, {}};
                }
                else result = failed("The document could not be reloaded.", pending->reloadDocument);
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
                        if (document.metadata().path) guard->fileMonitor_.monitor(document.id(), *document.metadata().path);
                        if (document.metadata().path) guard->scheduleExternalInspection(document.id());
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
    fileMonitor_.clear();
}

void DocumentWorkflow::setExternalFileChangeCallback(ExternalFileChangeCallback callback) {
    externalFileChangeCallback_ = std::move(callback);
}

void DocumentWorkflow::checkExternalChanges() {
    fileMonitor_.refresh();
    for (const auto id : tabs_.order()) scheduleExternalInspection(id);
}

void DocumentWorkflow::scheduleExternalInspection(documents::DocumentId id) {
    if (shuttingDown_) return;
    if (inspectingFiles_.contains(id)) {
        changedDuringInspection_.insert(id);
        return;
    }
    const auto* document = documents_.find(id);
    if (!document || !document->metadata().path || !document->metadata().file) return;
    const auto path = *document->metadata().path;
    const auto expected = document->metadata().file->sha256;
    inspectingFiles_.insert(id);
    QPointer<DocumentWorkflow> guard(this);
    backgroundPool().start([guard, id, path, expected] {
        const auto result = documents::FileService{}.inspect(path);
        if (!guard) return;
        QMetaObject::invokeMethod(guard, [guard, id, expected, result] {
            if (!guard) return;
            guard->inspectingFiles_.erase(id);
            if (!guard->shuttingDown_) {
                const auto* document = guard->documents_.find(id);
                const bool baselineChanged = !document || !document->metadata().file ||
                    document->metadata().file->sha256 != expected;
                if (baselineChanged) {
                    guard->scheduleExternalInspection(id);
                } else if (result.error.empty()) {
                    auto state = documents::ExternalFileState::changed;
                    if (!result.exists) state = documents::ExternalFileState::missing;
                    else if (result.metadata.sha256 == expected)
                        state = documents::ExternalFileState::current;
                    guard->updateExternalFileState(id, state);
                }
            }
            const auto retry = guard->changedDuringInspection_.erase(id) != 0;
            if (retry && !guard->shuttingDown_) guard->scheduleExternalInspection(id);
        }, Qt::QueuedConnection);
    });
}

void DocumentWorkflow::updateExternalFileState(documents::DocumentId id,
                                                documents::ExternalFileState state) {
    auto* document = documents_.find(id);
    if (!document || document->metadata().externalFileState == state) return;
    (void)documents_.setExternalFileState(id, state);
    if (externalFileChangeCallback_) externalFileChangeCallback_(id, state);
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
    if (result.conflict) {
        (void)documents_.setExternalFileState(id, result.missing ? documents::ExternalFileState::missing
                                                                : documents::ExternalFileState::changed);
        return {OperationStatus::conflict, id, result.error};
    }
    if (!result) return failed(result.error, id);
    (void)documents_.setExternalFileState(id, documents::ExternalFileState::current);
    if (document->metadata().path) fileMonitor_.monitor(id, *document->metadata().path);
    scheduleExternalInspection(id);
    return {OperationStatus::success, id, {}};
}

WorkflowResult DocumentWorkflow::saveAs(documents::DocumentId id,
                                        const std::filesystem::path& path, bool allowOverwrite,
                                        std::optional<std::array<std::uint8_t, 32>> expectedTargetFingerprint) {
    auto* document = documents_.find(id);
    if (!document) return {OperationStatus::notFound, id, {}};
    if (path.empty()) return {OperationStatus::cancelled, id, {}};
    if (const auto collision = documents_.findByPath(path, id)) {
        return failed("That file is already open in another tab.", id);
    }
    const auto result = files_.saveAs(*document, path, allowOverwrite, expectedTargetFingerprint);
    if (result.conflict) return {OperationStatus::conflict, id, result.error};
    if (!result) return failed(result.error, id);
    (void)documents_.setExternalFileState(id, documents::ExternalFileState::current);
    fileMonitor_.monitor(id, path);
    scheduleExternalInspection(id);
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
    fileMonitor_.remove(id);
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
