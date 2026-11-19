#include "ui/main_window.hpp"

#include "documents/document.hpp"
#include "platform/launch_services.hpp"
#include "platform/path_resolver.hpp"
#include "ui/editor_view.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMessageBox>
#include <QPushButton>
#include <QProgressDialog>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QUrl>

#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <set>
#include <string>

namespace note::ui {
namespace {

QString fromUtf8(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

} // namespace

MainWindow::MainWindow(application::DocumentWorkflow& workflow, QWidget* parent)
    : QMainWindow(parent), workflow_(workflow) {
    setAcceptDrops(true);
    setWindowTitle(tr("Note"));
    resize(1000, 700);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    tabs_ = new QTabBar(central);
    tabs_->setObjectName(QStringLiteral("documentTabs"));
    tabs_->setDocumentMode(true);
    tabs_->setTabsClosable(true);
    tabs_->setMovable(false);
    tabs_->setExpanding(false);
    tabs_->setUsesScrollButtons(true);
    layout->addWidget(tabs_);

    editorStack_ = new QStackedWidget(central);
    editorStack_->setObjectName(QStringLiteral("editorStack"));
    emptyEditor_ = new QLabel(tr("Create a document or open a file to get started."),
                              editorStack_);
    emptyEditor_->setObjectName(QStringLiteral("emptyEditorMessage"));
    static_cast<QLabel*>(emptyEditor_)->setAlignment(Qt::AlignCenter);
    editorStack_->addWidget(emptyEditor_);
    layout->addWidget(editorStack_, 1);
    setCentralWidget(central);

    status_ = new QLabel(this);
    statusBar()->addPermanentWidget(status_);
    createActions();
    workflow_.setExternalFileChangeCallback([this](documents::DocumentId id,
                                                    documents::ExternalFileState state) {
        showExternalFileChange(id, state);
    });

    connect(tabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0) {
            const auto value = tabs_->tabData(index).toULongLong();
            (void)workflow_.activate(static_cast<documents::DocumentId>(value));
        }
        refreshPresentation();
    });
    connect(tabs_, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (index < 0) return;
        const auto value = tabs_->tabData(index).toULongLong();
        closeDocument(static_cast<documents::DocumentId>(value));
    });
    refresh();
}

MainWindow::~MainWindow() { workflow_.setExternalFileChangeCallback({}); }

void MainWindow::createActions() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* newAction = fileMenu->addAction(tr("&New"));
    newAction->setObjectName(QStringLiteral("newDocumentAction"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] {
        (void)workflow_.createNew();
        refresh();
    });

    auto* openAction = fileMenu->addAction(tr("&Open…"));
    openAction->setObjectName(QStringLiteral("openDocumentAction"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] { openFile(); });

    fileMenu->addSeparator();
    saveAction_ = fileMenu->addAction(tr("&Save"));
    saveAction_->setObjectName(QStringLiteral("saveDocumentAction"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveActive(); });

    saveAsAction_ = fileMenu->addAction(tr("Save &As…"));
    saveAsAction_->setObjectName(QStringLiteral("saveAsDocumentAction"));
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered, this, [this] { saveActiveAs(); });

    fileMenu->addSeparator();
    closeAction_ = fileMenu->addAction(tr("&Close Tab"));
    closeAction_->setObjectName(QStringLiteral("closeDocumentAction"));
    closeAction_->setShortcut(QKeySequence::Close);
    connect(closeAction_, &QAction::triggered, this, [this] {
        if (const auto* active = workflow_.activeDocument()) closeDocument(active->id());
    });

    fileMenu->addSeparator();
    externalOpenAction_ = fileMenu->addAction(tr("Open in External Application"));
    externalOpenAction_->setObjectName(QStringLiteral("openExternalApplicationAction"));
    connect(externalOpenAction_, &QAction::triggered, this, [this] {
        const auto* document = workflow_.activeDocument();
        if (!document || !document->metadata().path) return;
        const auto result = platform::openInExternalApplication(*document->metadata().path);
        if (!result.success) showError(tr("Could not open the file externally: %1").arg(fromUtf8(result.error)));
    });
    terminalAction_ = fileMenu->addAction(tr("Open Terminal Here"));
    terminalAction_->setObjectName(QStringLiteral("openTerminalAction"));
    connect(terminalAction_, &QAction::triggered, this, [this] {
        const auto* document = workflow_.activeDocument();
        if (!document || !document->metadata().path) return;
        const auto directory = document->metadata().path->parent_path();
        const auto result = platform::openTerminalInDirectory(directory);
        if (!result.success) showError(tr("Could not open a terminal: %1").arg(fromUtf8(result.error)));
    });
}

QString MainWindow::displayName(const documents::Document& document) const {
    if (!document.metadata().path) {
        return tr("Untitled %1").arg(static_cast<qulonglong>(document.id()));
    }
    const auto fileName = QFileInfo(platform::pathToQString(*document.metadata().path)).fileName();
    return fileName.isEmpty() ? platform::pathToQString(*document.metadata().path) : fileName;
}

void MainWindow::refresh() {
    syncEditorViews();
    tabs_->blockSignals(true);
    while (tabs_->count() > 0) tabs_->removeTab(tabs_->count() - 1);
    int activeIndex = -1;
    for (const auto id : workflow_.tabs().order()) {
        const auto* document = workflow_.findDocument(id);
        if (!document) continue;
        QString label = displayName(*document);
        if (document->modified()) label += QLatin1Char('*');
        const auto index = tabs_->addTab(label);
        tabs_->setTabData(index, QVariant::fromValue<qulonglong>(id));
        if (document->metadata().path) tabs_->setTabToolTip(index, platform::pathToQString(*document->metadata().path));
        else tabs_->setTabToolTip(index, label);
        if (workflow_.activeDocumentId() == id) activeIndex = index;
    }
    if (activeIndex >= 0) tabs_->setCurrentIndex(activeIndex);
    tabs_->blockSignals(false);
    refreshPresentation();
}

void MainWindow::refreshFromModel() {
    refresh();
}

void MainWindow::refreshPresentation() {
    const auto* document = workflow_.activeDocument();
    const bool hasDocument = document != nullptr;
    auto activeView = editorViews_.find(document ? document->id() : 0);
    editorStack_->setCurrentWidget(
        hasDocument && activeView != editorViews_.end() ? activeView->second.view
                                                        : emptyEditor_);
    if (hasDocument && activeView != editorViews_.end())
        activeView->second.view->refreshFromModel();
    saveAction_->setEnabled(hasDocument);
    saveAsAction_->setEnabled(hasDocument);
    closeAction_->setEnabled(hasDocument);
    const bool hasPath = hasDocument && document->metadata().path.has_value();
    externalOpenAction_->setEnabled(hasPath);
    terminalAction_->setEnabled(hasPath);

    if (!document) {
        setWindowTitle(tr("Note"));
        status_->setText(tr("No document"));
        return;
    }

    const auto name = displayName(*document);
    setWindowTitle(tr("%1%2 — Note").arg(name, document->modified() ? QStringLiteral("*") : QString{}));
    status_->setText(tr("%1 lines · %2 bytes%3")
        .arg(static_cast<qulonglong>(document->buffer().lineCount()))
        .arg(static_cast<qulonglong>(document->buffer().sizeBytes()))
        .arg(document->modified() ? tr(" · Modified") : QString{}));
}

void MainWindow::syncEditorViews() {
    std::set<documents::DocumentId> live;
    for (const auto id : workflow_.tabs().order()) {
        auto* document = workflow_.findDocument(id);
        if (!document) continue;
        live.insert(id);
        auto found = editorViews_.find(id);
        if (found != editorViews_.end() &&
            found->second.contentGeneration == document->contentGeneration())
            continue;
        if (found != editorViews_.end()) {
            editorStack_->removeWidget(found->second.view);
            delete found->second.view;
            editorViews_.erase(found);
        }
        auto* view = new EditorView(document->buffer(),
                                    document->metadata().preferredLineEnding,
                                    editorStack_);
        view->setDocumentChangedCallback([this, id] { refreshAfterEdit(id); });
        editorStack_->addWidget(view);
        editorViews_.emplace(id, EditorBinding{document->contentGeneration(), view});
    }
    for (auto it = editorViews_.begin(); it != editorViews_.end();) {
        if (live.contains(it->first)) {
            ++it;
            continue;
        }
        editorStack_->removeWidget(it->second.view);
        delete it->second.view;
        it = editorViews_.erase(it);
    }
}

void MainWindow::refreshAfterEdit(documents::DocumentId id) {
    const auto* document = workflow_.findDocument(id);
    if (!document) return;
    const auto index = workflow_.tabs().indexOf(id);
    if (index && static_cast<int>(*index) < tabs_->count()) {
        auto label = displayName(*document);
        if (document->modified()) label += QLatin1Char('*');
        tabs_->setTabText(static_cast<int>(*index), label);
    }
    refreshPresentation();
}

void MainWindow::openFile() {
    const auto selected = QFileDialog::getOpenFileName(this, tr("Open File"), {}, tr("All Files (*)"));
    if (selected.isEmpty()) return;
    openPath(platform::pathFromQString(selected));
}

void MainWindow::openPath(const std::filesystem::path& path) {
    const auto request = workflow_.openAsync(path,
        [this](auto id, documents::OpenProgress update) { updateOpenProgress(id, update); },
        [this](auto id, application::WorkflowResult result) {
            finishOpenProgress(id);
            if (result.status == application::OperationStatus::failed)
                showError(tr("Could not open the file: %1").arg(fromUtf8(result.error)));
            if (result) refresh();
        });
    if (!request || openProgress_.contains(request)) return;
    createOpenProgress(request, tr("Opening file…"));
}

void MainWindow::createOpenProgress(application::DocumentWorkflow::OpenRequestId request,
                                    const QString& label) {
    auto* dialog = new QProgressDialog(label, tr("Cancel"), 0, 1000, this);
    dialog->setObjectName(QStringLiteral("openProgressDialog"));
    dialog->setWindowTitle(label.startsWith(tr("Reloading")) ? tr("Reloading File") : tr("Opening File"));
    dialog->setWindowModality(Qt::NonModal);
    dialog->setMinimumDuration(350);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    connect(dialog, &QProgressDialog::canceled, this, [this, request, dialog] {
        dialog->setLabelText(tr("Cancelling file open…"));
        dialog->setCancelButton(nullptr);
        workflow_.cancelOpen(request);
    });
    openProgress_.emplace(request, dialog);
}

void MainWindow::updateOpenProgress(application::DocumentWorkflow::OpenRequestId id,
                                    documents::OpenProgress update) {
    const auto it = openProgress_.find(id);
    if (it == openProgress_.end() || !it->second) return;
    auto* dialog = it->second;
    if (update.buildingIndex) {
        dialog->setLabelText(tr("Building document index…"));
        dialog->setRange(0, 0);
        return;
    }
    dialog->setLabelText(tr("Opening file… %1 of %2 MB")
        .arg(static_cast<qulonglong>(update.bytesRead / (1024 * 1024)))
        .arg(static_cast<qulonglong>(update.totalBytes / (1024 * 1024))));
    if (update.totalBytes > 0) {
        dialog->setRange(0, 1000);
        dialog->setValue(static_cast<int>(std::min<std::uintmax_t>(1000,
            update.bytesRead * 1000 / update.totalBytes)));
    } else dialog->setRange(0, 0);
}

void MainWindow::finishOpenProgress(application::DocumentWorkflow::OpenRequestId id) {
    if (const auto it = openProgress_.find(id); it != openProgress_.end()) {
        if (it->second) it->second->deleteLater();
        openProgress_.erase(it);
    }
}

void MainWindow::saveActive() {
    const auto* document = workflow_.activeDocument();
    if (!document) return;
    const auto id = document->id();
    auto result = workflow_.save(id);
    if (result.status == application::OperationStatus::needsSavePath) {
        saveDocumentAs(id);
        return;
    }
    if (result.status == application::OperationStatus::conflict) {
        showSaveConflict(id, fromUtf8(result.error));
        return;
    }
    if (result.status == application::OperationStatus::failed) {
        showError(tr("Could not save the file: %1").arg(fromUtf8(result.error)));
    } else if (result) {
        refresh();
    }
}

void MainWindow::saveActiveAs() {
    const auto* document = workflow_.activeDocument();
    if (!document) return;
    saveDocumentAs(document->id());
}

void MainWindow::saveDocumentAs(documents::DocumentId id) {
    const auto* document = workflow_.findDocument(id);
    if (!document) return;
    const auto path = requestSavePath(*document);
    if (!path) return;
    const auto destination = workflow_.inspectFile(*path);
    if (!destination.error.empty()) {
        showError(tr("Could not check the destination: %1").arg(fromUtf8(destination.error)));
        return;
    }
    bool replaceExisting = false;
    if (destination.exists) {
        QMessageBox prompt(QMessageBox::Warning, tr("Replace Existing File"),
            tr("A file already exists at this path. Replace it with the current document?"),
            QMessageBox::NoButton, this);
        auto* replaceButton = prompt.addButton(tr("Replace"), QMessageBox::DestructiveRole);
        auto* cancelButton = prompt.addButton(QMessageBox::Cancel);
        prompt.setDefaultButton(qobject_cast<QPushButton*>(cancelButton));
        prompt.exec();
        replaceExisting = prompt.clickedButton() == replaceButton;
        if (!replaceExisting) return;
    }
    const auto result = workflow_.saveAs(id, *path, replaceExisting,
        destination.exists ? std::optional{destination.metadata.sha256} : std::nullopt);
    if (result.status == application::OperationStatus::conflict) {
        showSaveConflict(id, fromUtf8(result.error));
        return;
    }
    if (result.status == application::OperationStatus::failed) {
        showError(tr("Could not save the file: %1").arg(fromUtf8(result.error)));
    } else if (result) {
        refresh();
    }
}

std::optional<std::filesystem::path> MainWindow::requestSavePath(
    const documents::Document& document) {
    QString suggested;
    if (document.metadata().path) suggested = platform::pathToQString(*document.metadata().path);
    const auto selected = QFileDialog::getSaveFileName(this, tr("Save File As"), suggested,
                                                        tr("All Files (*)"));
    if (selected.isEmpty()) return std::nullopt;
    return platform::pathFromQString(selected);
}

void MainWindow::closeDocument(documents::DocumentId id) {
    const auto* document = workflow_.findDocument(id);
    if (!document) return;
    auto choice = application::CloseChoice::cancel;
    std::optional<std::filesystem::path> savePath;
    if (document->modified()) {
        QMessageBox prompt(QMessageBox::Warning, tr("Unsaved Changes"),
                           tr("Save changes to %1 before closing?").arg(displayName(*document)),
                           QMessageBox::NoButton, this);
        auto* saveButton = prompt.addButton(QMessageBox::Save);
        auto* discardButton = prompt.addButton(QMessageBox::Discard);
        auto* cancelButton = prompt.addButton(QMessageBox::Cancel);
        prompt.setDefaultButton(qobject_cast<QPushButton*>(saveButton));
        prompt.exec();
        if (prompt.clickedButton() == saveButton) choice = application::CloseChoice::save;
        else if (prompt.clickedButton() == discardButton) choice = application::CloseChoice::discard;
        else if (prompt.clickedButton() == cancelButton) choice = application::CloseChoice::cancel;
        if (choice == application::CloseChoice::cancel) return;
        if (choice == application::CloseChoice::save && !document->metadata().path) {
            savePath = requestSavePath(*document);
            if (!savePath) return;
        }
    }
    const auto result = workflow_.close(id, choice, std::move(savePath));
    if (result.status == application::OperationStatus::conflict) {
        showSaveConflict(id, fromUtf8(result.error));
    } else if (result.status == application::OperationStatus::failed) {
        showError(tr("Could not close the document: %1").arg(fromUtf8(result.error)));
    }
    refresh();
}

bool MainWindow::resolveAllDocumentsForWindowClose() {
    struct Decision { documents::DocumentId id; application::CloseChoice choice; };
    std::vector<Decision> decisions;
    decisions.reserve(workflow_.tabs().size());
    const auto ids = workflow_.tabs().order();
    for (const auto id : ids) {
        const auto* document = workflow_.findDocument(id);
        if (!document || !document->modified()) {
            decisions.push_back({id, application::CloseChoice::cancel});
            continue;
        }
        QMessageBox prompt(QMessageBox::Warning, tr("Unsaved Changes"),
                           tr("Save changes to %1 before closing Note?").arg(displayName(*document)),
                           QMessageBox::NoButton, this);
        auto* saveButton = prompt.addButton(QMessageBox::Save);
        auto* discardButton = prompt.addButton(QMessageBox::Discard);
        auto* cancelButton = prompt.addButton(QMessageBox::Cancel);
        prompt.setDefaultButton(qobject_cast<QPushButton*>(saveButton));
        prompt.exec();
        if (prompt.clickedButton() == cancelButton) return false;
        if (prompt.clickedButton() == discardButton) {
            decisions.push_back({id, application::CloseChoice::discard});
            continue;
        }
        if (prompt.clickedButton() != saveButton) return false;

        application::WorkflowResult saved;
        if (document->metadata().path) {
            saved = workflow_.save(id);
        } else {
            const auto path = requestSavePath(*document);
            if (!path) return false;
            saved = workflow_.saveAs(id, *path);
        }
        if (!saved) {
            if (saved.status == application::OperationStatus::conflict)
                showSaveConflict(id, fromUtf8(saved.error));
            else showError(tr("Could not save the file: %1").arg(fromUtf8(saved.error)));
            return false;
        }
        decisions.push_back({id, application::CloseChoice::cancel});
    }

    for (const auto& decision : decisions) {
        const auto result = workflow_.close(decision.id, decision.choice);
        if (!result) {
            showError(tr("Could not close a document: %1").arg(fromUtf8(result.error)));
            return false;
        }
    }
    refresh();
    return true;
}

void MainWindow::showError(const QString& message) {
    QMessageBox::critical(this, tr("Note"), message);
}

void MainWindow::startReload(documents::DocumentId id, bool confirmDiscardLocalChanges) {
    const auto* document = workflow_.findDocument(id);
    if (!document) return;
    if (document->modified() && !confirmDiscardLocalChanges) {
        auto* prompt = new QMessageBox(QMessageBox::Warning, tr("Discard Local Changes?"),
            tr("This document now has unsaved local changes. Reloading will discard them. Continue?"),
            QMessageBox::NoButton, this);
        prompt->setAttribute(Qt::WA_DeleteOnClose);
        auto* reload = prompt->addButton(tr("Reload and Discard"), QMessageBox::DestructiveRole);
        prompt->addButton(tr("Keep Editing"), QMessageBox::RejectRole);
        connect(prompt, &QMessageBox::buttonClicked, this, [this, prompt, reload, id](QAbstractButton* button) {
            if (button == reload) startReload(id, true);
            prompt->close();
        });
        prompt->open();
        return;
    }
    const auto request = workflow_.reloadAsync(id, confirmDiscardLocalChanges,
        [this](auto requestId, documents::OpenProgress update) { updateOpenProgress(requestId, update); },
        [this](auto requestId, application::WorkflowResult result) {
            finishOpenProgress(requestId);
            if (result.status == application::OperationStatus::conflict)
                showSaveConflict(*result.documentId, fromUtf8(result.error));
            else if (result.status == application::OperationStatus::failed)
                showError(tr("Could not reload the file: %1").arg(fromUtf8(result.error)));
            else if (result) refresh();
        });
    if (!request) {
        showError(tr("The document could not be reloaded because it changed or is no longer available."));
        return;
    }
    if (!openProgress_.contains(request)) createOpenProgress(request, tr("Reloading file…"));
}

void MainWindow::showExternalFileChange(documents::DocumentId id,
                                        documents::ExternalFileState state) {
    if (state == documents::ExternalFileState::current) return;
    const auto* document = workflow_.findDocument(id);
    if (!document) return;
    const bool missing = state == documents::ExternalFileState::missing;
    const bool modified = document->modified();
    auto* prompt = new QMessageBox(missing ? QMessageBox::Warning : QMessageBox::Information,
        missing ? tr("File No Longer Available") : tr("File Changed on Disk"),
        missing
            ? tr("%1 was removed or moved. The open document and its contents are preserved.")
                .arg(displayName(*document))
            : modified
                ? tr("%1 changed outside Note. Your unsaved local edits are preserved until you choose to reload.")
                    .arg(displayName(*document))
                : tr("%1 changed outside Note. Reload the latest disk contents now or later?")
                    .arg(displayName(*document)),
        QMessageBox::NoButton, this);
    prompt->setAttribute(Qt::WA_DeleteOnClose);
    QAbstractButton* primary = nullptr;
    if (missing) primary = prompt->addButton(tr("Save As…"), QMessageBox::ActionRole);
    else primary = prompt->addButton(modified ? tr("Reload and Discard Local Edits") : tr("Reload"),
                                     modified ? QMessageBox::DestructiveRole : QMessageBox::AcceptRole);
    auto* defer = prompt->addButton(missing ? tr("Keep Open") : modified ? tr("Keep Editing") : tr("Later"),
                                    QMessageBox::RejectRole);
    connect(prompt, &QMessageBox::buttonClicked, this,
        [this, prompt, primary, defer, id, missing, modified](QAbstractButton* button) {
            if (button == primary) {
                if (missing) saveDocumentAs(id);
                else startReload(id, modified);
            }
            if (button == defer || button == primary) prompt->close();
        });
    prompt->open();
}

void MainWindow::showSaveConflict(documents::DocumentId id, const QString& message) {
    const auto* document = workflow_.findDocument(id);
    if (!document) return;
    const bool missing = document->metadata().externalFileState == documents::ExternalFileState::missing;
    auto* prompt = new QMessageBox(QMessageBox::Warning, tr("Save Conflict"),
        tr("%1\n\nYour in-memory contents are safe. Choose Save As to keep them at another path, or reload the disk copy explicitly.")
            .arg(message), QMessageBox::NoButton, this);
    prompt->setAttribute(Qt::WA_DeleteOnClose);
    auto* saveAs = prompt->addButton(tr("Save As…"), QMessageBox::ActionRole);
    QAbstractButton* reload = nullptr;
    if (!missing) reload = prompt->addButton(document->modified()
        ? tr("Reload and Discard Local Edits") : tr("Reload"), QMessageBox::DestructiveRole);
    auto* keep = prompt->addButton(tr("Keep Current Document"), QMessageBox::RejectRole);
    connect(prompt, &QMessageBox::buttonClicked, this,
        [this, prompt, saveAs, reload, keep, id](QAbstractButton* button) {
            if (button == saveAs) saveDocumentAs(id);
            else if (button == reload) startReload(id, true);
            if (button == saveAs || button == reload || button == keep) prompt->close();
        });
    prompt->open();
}

bool MainWindow::event(QEvent* event) {
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::ApplicationActivate)
        workflow_.checkExternalChanges();
    return QMainWindow::event(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent* event) {
    bool openedAny = false;
    for (const auto& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        openPath(platform::pathFromQString(url.toLocalFile()));
        openedAny = true;
    }
    if (openedAny) event->acceptProposedAction();
    else QMainWindow::dropEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (closingResolved_ || resolveAllDocumentsForWindowClose()) {
        workflow_.shutdown();
        closingResolved_ = true;
        event->accept();
    } else {
        event->ignore();
    }
}

} // namespace note::ui
