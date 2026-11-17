#include "ui/main_window.hpp"

#include "documents/document.hpp"
#include "ui/document_preview.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProgressDialog>
#include <QStatusBar>
#include <QTabBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>

namespace note::ui {
namespace {

QString toQString(const std::filesystem::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.native());
#else
    const auto value = path.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(value.data()),
                             static_cast<qsizetype>(value.size()));
#endif
}

std::filesystem::path toPath(const QString& value) {
#if defined(_WIN32)
    return std::filesystem::path(value.toStdWString());
#else
    const auto bytes = value.toUtf8();
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t*>(bytes.constData()),
        static_cast<std::size_t>(bytes.size())));
#endif
}

QString fromUtf8(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

} // namespace

MainWindow::MainWindow(application::DocumentWorkflow& workflow, QWidget* parent)
    : QMainWindow(parent), workflow_(workflow) {
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

    preview_ = new DocumentPreview(central);
    preview_->setObjectName(QStringLiteral("documentPreview"));
    layout->addWidget(preview_, 1);
    setCentralWidget(central);

    status_ = new QLabel(this);
    statusBar()->addPermanentWidget(status_);
    createActions();

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
}

QString MainWindow::displayName(const documents::Document& document) const {
    if (!document.metadata().path) {
        return tr("Untitled %1").arg(static_cast<qulonglong>(document.id()));
    }
    const auto fileName = QFileInfo(toQString(*document.metadata().path)).fileName();
    return fileName.isEmpty() ? toQString(*document.metadata().path) : fileName;
}

void MainWindow::refresh() {
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
        if (document->metadata().path) tabs_->setTabToolTip(index, toQString(*document->metadata().path));
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
    preview_->showDocument(document);
    const bool hasDocument = document != nullptr;
    saveAction_->setEnabled(hasDocument);
    saveAsAction_->setEnabled(hasDocument);
    closeAction_->setEnabled(hasDocument);

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

void MainWindow::openFile() {
    const auto selected = QFileDialog::getOpenFileName(this, tr("Open File"), {}, tr("All Files (*)"));
    if (selected.isEmpty()) return;
    openPath(toPath(selected));
}

void MainWindow::openPath(const std::filesystem::path& path) {
    const auto request = workflow_.openAsync(path,
        [this](auto id, documents::OpenProgress update) {
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
                dialog->setValue(static_cast<int>(std::min<std::uintmax_t>(1000, update.bytesRead * 1000 / update.totalBytes)));
            } else dialog->setRange(0, 0);
        },
        [this](auto id, application::WorkflowResult result) {
            if (const auto it = openProgress_.find(id); it != openProgress_.end()) {
                if (it->second) it->second->deleteLater();
                openProgress_.erase(it);
            }
            if (result.status == application::OperationStatus::failed)
                showError(tr("Could not open the file: %1").arg(fromUtf8(result.error)));
            if (result) refresh();
        });
    if (!request || openProgress_.contains(request)) return;
    auto* dialog = new QProgressDialog(tr("Opening file…"), tr("Cancel"), 0, 1000, this);
    dialog->setObjectName(QStringLiteral("openProgressDialog"));
    dialog->setWindowTitle(tr("Opening File"));
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

void MainWindow::saveActive() {
    const auto* document = workflow_.activeDocument();
    if (!document) return;
    auto result = workflow_.save(document->id());
    if (result.status == application::OperationStatus::needsSavePath) {
        const auto path = requestSavePath(*document);
        if (!path) return;
        result = workflow_.saveAs(document->id(), *path);
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
    const auto path = requestSavePath(*document);
    if (!path) return;
    const auto result = workflow_.saveAs(document->id(), *path);
    if (result.status == application::OperationStatus::failed) {
        showError(tr("Could not save the file: %1").arg(fromUtf8(result.error)));
    } else if (result) {
        refresh();
    }
}

std::optional<std::filesystem::path> MainWindow::requestSavePath(
    const documents::Document& document) {
    QString suggested;
    if (document.metadata().path) suggested = toQString(*document.metadata().path);
    const auto selected = QFileDialog::getSaveFileName(this, tr("Save File As"), suggested,
                                                        tr("All Files (*)"));
    if (selected.isEmpty()) return std::nullopt;
    return toPath(selected);
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
    if (result.status == application::OperationStatus::failed) {
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
            showError(tr("Could not save the file: %1").arg(fromUtf8(saved.error)));
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
