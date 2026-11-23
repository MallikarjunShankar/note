#include "ui/main_window.hpp"

#include "documents/document.hpp"
#include "platform/launch_services.hpp"
#include "platform/path_resolver.hpp"
#include "commands/shortcut_dispatcher.hpp"
#include "ui/command_palette.hpp"
#include "ui/editor_view.hpp"
#include "ui/search_panel.hpp"
#include "ui/settings_dialog.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMessageBox>
#include <QPushButton>
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStandardPaths>
#include <QDir>
#include <QTabBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QToolBar>
#include <QPalette>
#include <QFontMetrics>
#include <QTimer>
#include <QUuid>
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

MainWindow::MainWindow(application::DocumentWorkflow& workflow,
                       application::SessionRecoveryService* session,
                       application::SessionState startupState,
                       QWidget* parent)
    : QMainWindow(parent), workflow_(workflow),
      settingsManager_(QDir(QStandardPaths::writableLocation(
                             QStandardPaths::AppDataLocation)).filePath(QStringLiteral("settings.ini"))),
      themeManager_(QDir(QStandardPaths::writableLocation(
                             QStandardPaths::AppDataLocation)).filePath(QStringLiteral("themes")), this),
      keybindingManager_(QDir(QStandardPaths::writableLocation(
                              QStandardPaths::AppDataLocation)).filePath(QStringLiteral("keybindings.ini"))),
      shortcutDispatcher_(commandRegistry_, keybindingManager_),
      sessionRecovery_(session), startupSession_(std::move(startupState)) {
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
    settingsManager_.load();
    searchPanel_ = new SearchPanel(this);
    searchPanel_->hide();
    createActions();
    applyCustomisation();
    workflow_.setExternalFileChangeCallback([this](documents::DocumentId id,
                                                    documents::ExternalFileState state) {
        showExternalFileChange(id, state);
    });
    workflow_.setExternalFileInspectionErrorCallback(
        [this](documents::DocumentId id, const std::string& error) {
            const auto* document = workflow_.findDocument(id);
            const auto name = document && document->metadata().path
                ? platform::pathToQString(*document->metadata().path)
                : tr("document");
            status_->setText(tr("Could not check %1: %2")
                                 .arg(name, fromUtf8(error)));
        });
    if (sessionRecovery_) {
        connect(sessionRecovery_, &application::SessionRecoveryService::persistenceError,
                this, [this](const QString& message) {
                    status_->setText(tr("Recovery warning: %1").arg(message));
                });
        sessionRecovery_->setProviders(
            [this] { return captureSessionState(); },
            [this] { return captureRecoveryDocuments(); });
        QTimer::singleShot(0, this, [this] {
            resolveStartupRecovery();
            restoreSessionDocuments();
        });
    }

    connect(tabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0) {
            const auto value = tabs_->tabData(index).toULongLong();
            (void)workflow_.activate(static_cast<documents::DocumentId>(value));
        }
        refreshPresentation();
        if (sessionRecovery_) sessionRecovery_->scheduleSessionSave();
    });
    connect(tabs_, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (index < 0) return;
        const auto value = tabs_->tabData(index).toULongLong();
        closeDocument(static_cast<documents::DocumentId>(value));
    });
    refresh();
}

MainWindow::~MainWindow() {
    workflow_.setExternalFileChangeCallback({});
    workflow_.setExternalFileInspectionErrorCallback({});
    if (sessionRecovery_) sessionRecovery_->clearProviders();
}

void MainWindow::createActions() {
    keybindingManager_.setDefault("file.new", QKeySequence::New);
    keybindingManager_.setDefault("file.open", QKeySequence::Open);
    keybindingManager_.setDefault("file.save", QKeySequence::Save);
    keybindingManager_.setDefault("file.saveAs", QKeySequence::SaveAs);
    keybindingManager_.setDefault("file.close", QKeySequence::Close);
    keybindingManager_.setDefault("file.external", {});
    keybindingManager_.setDefault("file.terminal", {});
    keybindingManager_.setDefault("edit.undo", QKeySequence::Undo);
    keybindingManager_.setDefault("edit.redo", QKeySequence::Redo);
    keybindingManager_.setDefault("search.find", QKeySequence::Find);
    keybindingManager_.setDefault("search.replace", QKeySequence::Replace);
    keybindingManager_.setDefault("search.next", QKeySequence::FindNext);
    keybindingManager_.setDefault("search.previous", QKeySequence::FindPrevious);
    keybindingManager_.setDefault("edit.indent", {});
    keybindingManager_.setDefault("edit.outdent", {});
    keybindingManager_.setDefault("view.nextTab", QKeySequence(Qt::CTRL | Qt::Key_Tab));
    keybindingManager_.setDefault("view.previousTab",
                                  QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    keybindingManager_.setDefault("view.lineNumbers", {});
    keybindingManager_.setDefault("view.whitespace", {});
    keybindingManager_.setDefault("view.indentationGuides", {});
    keybindingManager_.setDefault("view.wordWrap", {});
    keybindingManager_.setDefault("settings.open", QKeySequence(Qt::CTRL | Qt::Key_Comma));
    keybindingManager_.setDefault("commands.palette",
                                  QKeySequence(Qt::CTRL | Qt::Key_P));
    keybindingManager_.load();

    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto registerAction = [this](QAction* action, const char* id,
                                 const char* category, const char* description,
                                 commands::CommandRegistry::Enabled enabled = {}) {
        const auto commandId = std::string(id);
        action->setObjectName(QString::fromLatin1(id).replace(QLatin1Char('.'), QLatin1Char('_')) +
                              QStringLiteral("Action"));
        action->setShortcut(keybindingManager_.binding(commandId));
        commandActions_[commandId] = action;
        commandRegistry_.registerCommand(
            {commandId, action->text().remove(QLatin1Char('&')).toStdString(),
             category, description},
            [action] { action->trigger(); }, std::move(enabled));
    };

    auto* newAction = fileMenu->addAction(tr("&New"));
    registerAction(newAction, "file.new", "File", "Create a new document");
    newAction->setObjectName(QStringLiteral("newDocumentAction"));
    connect(newAction, &QAction::triggered, this, [this] {
        (void)workflow_.createNew();
        refresh();
    });

    auto* openAction = fileMenu->addAction(tr("&Open…"));
    registerAction(openAction, "file.open", "File", "Open a file");
    openAction->setObjectName(QStringLiteral("openDocumentAction"));
    connect(openAction, &QAction::triggered, this, [this] { openFile(); });

    fileMenu->addSeparator();
    saveAction_ = fileMenu->addAction(tr("&Save"));
    registerAction(saveAction_, "file.save", "File", "Save the active document",
                   [this] { return workflow_.activeDocument() != nullptr; });
    saveAction_->setObjectName(QStringLiteral("saveDocumentAction"));
    connect(saveAction_, &QAction::triggered, this, [this] { saveActive(); });

    saveAsAction_ = fileMenu->addAction(tr("Save &As…"));
    registerAction(saveAsAction_, "file.saveAs", "File", "Save the active document as",
                   [this] { return workflow_.activeDocument() != nullptr; });
    saveAsAction_->setObjectName(QStringLiteral("saveAsDocumentAction"));
    connect(saveAsAction_, &QAction::triggered, this, [this] { saveActiveAs(); });

    fileMenu->addSeparator();
    closeAction_ = fileMenu->addAction(tr("&Close Tab"));
    registerAction(closeAction_, "file.close", "File", "Close the active document",
                   [this] { return workflow_.activeDocument() != nullptr; });
    closeAction_->setObjectName(QStringLiteral("closeDocumentAction"));
    connect(closeAction_, &QAction::triggered, this, [this] {
        if (const auto* active = workflow_.activeDocument()) closeDocument(active->id());
    });

    fileMenu->addSeparator();
    externalOpenAction_ = fileMenu->addAction(tr("Open in External Application"));
    registerAction(externalOpenAction_, "file.external",
                   "File", "Open the active file in an external application",
                   [this] {
                       const auto* document = workflow_.activeDocument();
                       return document && document->metadata().path.has_value();
                   });
    connect(externalOpenAction_, &QAction::triggered, this, [this] {
        const auto* document = workflow_.activeDocument();
        if (!document || !document->metadata().path) return;
        const auto result = platform::openInExternalApplication(*document->metadata().path);
        if (!result.success) showError(tr("Could not open the file externally: %1").arg(fromUtf8(result.error)));
    });
    terminalAction_ = fileMenu->addAction(tr("Open Terminal Here"));
    registerAction(terminalAction_, "file.terminal",
                   "File", "Open a terminal in the active document directory",
                   [this] {
                       const auto* document = workflow_.activeDocument();
                       return document && document->metadata().path.has_value();
                   });
    connect(terminalAction_, &QAction::triggered, this, [this] {
        const auto* document = workflow_.activeDocument();
        if (!document || !document->metadata().path) return;
        const auto directory = document->metadata().path->parent_path();
        const auto result = platform::openTerminalInDirectory(directory);
        if (!result.success) showError(tr("Could not open a terminal: %1").arg(fromUtf8(result.error)));
    });

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    const auto hasEditor = [this] { return activeEditorView() != nullptr; };
    auto* undoAction = editMenu->addAction(tr("&Undo"));
    registerAction(undoAction, "edit.undo", "Edit", "Undo the last edit",
                   [this] { auto* view = activeEditorView(); return view && view->canUndo(); });
    connect(undoAction, &QAction::triggered, this, [this] {
        if (auto* view = activeEditorView()) (void)view->undo();
    });
    auto* redoAction = editMenu->addAction(tr("&Redo"));
    registerAction(redoAction, "edit.redo", "Edit", "Redo the last undone edit",
                   [this] { auto* view = activeEditorView(); return view && view->canRedo(); });
    connect(redoAction, &QAction::triggered, this, [this] {
        if (auto* view = activeEditorView()) (void)view->redo();
    });
    editMenu->addSeparator();
    auto* indentAction = editMenu->addAction(tr("&Indent Lines"));
    registerAction(indentAction, "edit.indent", "Edit",
                   "Indent the current or selected lines (also Tab)", hasEditor);
    connect(indentAction, &QAction::triggered, this, [this] {
        if (auto* view = activeEditorView()) view->indentSelectedLines();
    });
    auto* outdentAction = editMenu->addAction(tr("&Outdent Lines"));
    registerAction(outdentAction, "edit.outdent", "Edit",
                   "Outdent the current or selected lines (also Shift+Tab)", hasEditor);
    connect(outdentAction, &QAction::triggered, this, [this] {
        if (auto* view = activeEditorView()) view->outdentSelectedLines();
    });
    editMenu->addSeparator();
    auto* findAction = editMenu->addAction(tr("&Find…"));
    registerAction(findAction, "search.find", "Search", "Find text in the active document",
                   hasEditor);
    connect(findAction, &QAction::triggered, this, [this] { showFind(); });
    auto* findNextAction = editMenu->addAction(tr("Find &Next"));
    registerAction(findNextAction, "search.next", "Search",
                   "Select the next match of the current search", hasEditor);
    connect(findNextAction, &QAction::triggered, this, [this] {
        if (!searchPanel_) return;
        searchPanel_->setActiveView(activeEditorView());
        searchPanel_->findNext();
    });
    auto* findPreviousAction = editMenu->addAction(tr("Find Pre&vious"));
    registerAction(findPreviousAction, "search.previous", "Search",
                   "Select the previous match of the current search", hasEditor);
    connect(findPreviousAction, &QAction::triggered, this, [this] {
        if (!searchPanel_) return;
        searchPanel_->setActiveView(activeEditorView());
        searchPanel_->findPrevious();
    });
    auto* replaceAction = editMenu->addAction(tr("&Replace…"));
    registerAction(replaceAction, "search.replace", "Search",
                   "Find and replace text in the active document", hasEditor);
    connect(replaceAction, &QAction::triggered, this, [this] { showReplace(); });

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    auto* lineNumbers = viewMenu->addAction(tr("Line Numbers"));
    lineNumbers->setCheckable(true);
    lineNumbers->setChecked(settingsManager_.settings().lineNumbers);
    registerAction(lineNumbers, "view.lineNumbers", "View", "Toggle line numbers");
    connect(lineNumbers, &QAction::toggled, this, [this](bool checked) {
        auto value = settingsManager_.settings();
        value.lineNumbers = checked;
        settingsManager_.setSettings(value);
        settingsManager_.save();
        applyCustomisation();
    });
    auto* whitespace = viewMenu->addAction(tr("Whitespace"));
    whitespace->setCheckable(true);
    whitespace->setChecked(settingsManager_.settings().whitespaceVisible);
    registerAction(whitespace, "view.whitespace", "View", "Toggle whitespace markers");
    connect(whitespace, &QAction::toggled, this, [this](bool checked) {
        auto value = settingsManager_.settings();
        value.whitespaceVisible = checked;
        settingsManager_.setSettings(value);
        settingsManager_.save();
        applyCustomisation();
    });
    auto* guides = viewMenu->addAction(tr("Indentation Guides"));
    guides->setCheckable(true);
    guides->setChecked(settingsManager_.settings().indentationGuides);
    registerAction(guides, "view.indentationGuides", "View", "Toggle indentation guides");
    connect(guides, &QAction::toggled, this, [this](bool checked) {
        auto value = settingsManager_.settings();
        value.indentationGuides = checked;
        settingsManager_.setSettings(value);
        settingsManager_.save();
        applyCustomisation();
    });
    auto* wrap = viewMenu->addAction(tr("Word Wrap"));
    wrap->setCheckable(true);
    wrap->setChecked(settingsManager_.settings().wordWrap);
    registerAction(wrap, "view.wordWrap", "View", "Toggle word wrapping");
    connect(wrap, &QAction::toggled, this, [this](bool checked) {
        auto value = settingsManager_.settings();
        value.wordWrap = checked;
        settingsManager_.setSettings(value);
        settingsManager_.save();
        applyCustomisation();
    });

    viewMenu->addSeparator();
    const auto hasSeveralTabs = [this] { return workflow_.tabs().size() > 1; };
    auto* nextTab = viewMenu->addAction(tr("Next &Tab"));
    registerAction(nextTab, "view.nextTab", "View", "Switch to the next document tab",
                   hasSeveralTabs);
    connect(nextTab, &QAction::triggered, this, [this] {
        if (tabs_->count() > 1) tabs_->setCurrentIndex((tabs_->currentIndex() + 1) % tabs_->count());
    });
    auto* previousTab = viewMenu->addAction(tr("Previous T&ab"));
    registerAction(previousTab, "view.previousTab", "View",
                   "Switch to the previous document tab", hasSeveralTabs);
    connect(previousTab, &QAction::triggered, this, [this] {
        const int count = tabs_->count();
        if (count > 1) tabs_->setCurrentIndex((tabs_->currentIndex() + count - 1) % count);
    });

    auto* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    auto* settingsAction = settingsMenu->addAction(tr("&Preferences"));
    registerAction(settingsAction, "settings.open", "Settings", "Open settings");
    connect(settingsAction, &QAction::triggered, this, [this] { showSettings(); });
    auto* paletteAction = settingsMenu->addAction(tr("Command &Palette"));
    registerAction(paletteAction, "commands.palette", "Commands", "Search and execute commands");
    connect(paletteAction, &QAction::triggered, this, [this] {
        CommandPalette palette(commandRegistry_, this, &keybindingManager_);
        palette.exec();
    });

    toolBar_ = addToolBar(tr("Main Toolbar"));
    toolBar_->setObjectName(QStringLiteral("mainToolbar"));
    toolBar_->setMovable(false);
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
    if (sessionRecovery_) {
        sessionRecovery_->scheduleSessionSave();
        sessionRecovery_->scheduleRecoverySnapshot();
    }
}

EditorView* MainWindow::activeEditorView() {
    const auto* document = workflow_.activeDocument();
    if (!document) return nullptr;
    const auto found = editorViews_.find(document->id());
    return found == editorViews_.end() ? nullptr : found->second.view;
}

application::SessionState MainWindow::captureSessionState() const {
    application::SessionState state;
    for (const auto id : workflow_.tabs().order()) {
        const auto* document = workflow_.findDocument(id);
        if (!document) continue;
        const auto key = recoveryKeys_.find(id);
        const auto view = editorViews_.find(id);
        state.documents.push_back({
            document->metadata().path
                ? platform::pathToQString(*document->metadata().path)
                : QString{},
            key == recoveryKeys_.end() ? QString{} : key->second,
            view == editorViews_.end() ? 0
                : static_cast<std::uint64_t>(view->second.view->cursors().primaryOffset())});
        if (workflow_.activeDocumentId() == id)
            state.activeIndex = static_cast<int>(state.documents.size() - 1);
    }
    return state;
}

std::vector<application::RecoveryDocument>
MainWindow::captureRecoveryDocuments() {
    std::vector<application::RecoveryDocument> result;
    for (const auto id : workflow_.tabs().order()) {
        const auto* document = workflow_.findDocument(id);
        if (!document || !document->modified()) continue;
        const auto key = recoveryKeys_.find(id);
        if (key == recoveryKeys_.end()) continue;
        result.push_back({
            key->second,
            document->metadata().path
                ? platform::pathToQString(*document->metadata().path)
                : QString{},
            &document->buffer(),
            document->buffer().revision()});
    }
    return result;
}

void MainWindow::resolveStartupRecovery() {
    if (!sessionRecovery_) return;
    if (!startupSession_.error.isEmpty())
        status_->setText(startupSession_.error);
    if (startupSession_.previousRunClean) return;
    const auto snapshots = sessionRecovery_->recoveries();
    if (snapshots.empty()) return;
    QStringList details;
    bool hasValid = false;
    for (const auto& snapshot : snapshots) {
        if (snapshot.contents) {
            hasValid = true;
            recoveryKeysHandledAtStartup_.insert(snapshot.key);
            details.push_back(snapshot.sourcePath.isEmpty()
                ? tr("Unsaved document") : snapshot.sourcePath);
        } else {
            details.push_back(tr("Unavailable recovery data: %1")
                                  .arg(snapshot.error));
        }
    }
    QMessageBox prompt(QMessageBox::Warning, tr("Recover Previous Session"),
        tr("Note did not shut down normally. Recovery copies are stored separately and will not overwrite original files."),
        QMessageBox::NoButton, this);
    prompt.setInformativeText(details.join(QLatin1Char('\n')));
    auto* recover = hasValid
        ? prompt.addButton(tr("Recover as Untitled"), QMessageBox::AcceptRole)
        : nullptr;
    auto* discard = prompt.addButton(tr("Discard Recovery Data"), QMessageBox::DestructiveRole);
    auto* later = prompt.addButton(tr("Later"), QMessageBox::RejectRole);
    prompt.exec();
    if (prompt.clickedButton() == recover) {
        for (const auto& snapshot : snapshots) {
            if (!snapshot.contents) continue;
            auto& document = workflow_.createNew();
            const auto key = snapshot.key;
            recoveryKeys_[document.id()] = key;
            document.buffer().replace(0, 0, *snapshot.contents);
            refresh();
            if (const auto view = editorViews_.find(document.id());
                view != editorViews_.end()) {
                for (const auto& saved : startupSession_.documents) {
                    if (saved.recoveryKey == key) {
                        view->second.view->restoreCursorPosition(
                            static_cast<std::size_t>(saved.cursorOffset));
                        break;
                    }
                }
            }
        }
        sessionRecovery_->flushRecoverySnapshotsNow();
    } else if (prompt.clickedButton() == discard) {
        for (const auto& snapshot : snapshots)
            sessionRecovery_->removeRecovery(snapshot.key);
        recoveryKeysHandledAtStartup_.clear();
    } else if (prompt.clickedButton() == later) {
        preserveRecoveryForNextStartup_ = true;
    }
}

void MainWindow::restoreSessionDocuments() {
    if (!sessionRecovery_ || startupSession_.documents.empty()) return;
    restoredDocumentIds_.assign(startupSession_.documents.size(), 0);
    restoreSessionDocumentAt(0);
}

void MainWindow::restoreSessionDocumentAt(std::size_t index) {
    if (index >= startupSession_.documents.size()) {
        if (startupSession_.activeIndex >= 0 &&
            startupSession_.activeIndex < static_cast<int>(restoredDocumentIds_.size())) {
            auto active = restoredDocumentIds_[static_cast<std::size_t>(startupSession_.activeIndex)];
            if (active == 0) {
                const auto found = std::find_if(
                    restoredDocumentIds_.begin(), restoredDocumentIds_.end(),
                    [](documents::DocumentId id) { return id != 0; });
                if (found != restoredDocumentIds_.end()) active = *found;
            }
            if (active != 0) {
                (void)workflow_.activate(active);
                refreshPresentation();
            }
        }
        return;
    }
    const auto saved = startupSession_.documents[index];
    if (saved.path.isEmpty()) {
        if (recoveryKeysHandledAtStartup_.contains(saved.recoveryKey)) {
            restoreSessionDocumentAt(index + 1);
            return;
        }
        auto& document = workflow_.createNew();
        recoveryKeys_[document.id()] = saved.recoveryKey;
        restoredDocumentIds_[index] = document.id();
        refresh();
        restoreSessionDocumentAt(index + 1);
        return;
    }
    const auto path = platform::pathFromQString(saved.path);
    const QFileInfo fileInfo(platform::pathToQString(path));
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        status_->setText(tr("Could not restore %1; the file is missing or inaccessible.")
                             .arg(saved.path));
        restoreSessionDocumentAt(index + 1);
        return;
    }
    const auto request = workflow_.openAsync(path, {}, [this, index, saved](auto,
            application::WorkflowResult result) {
        if (result && result.documentId) {
            recoveryKeys_[*result.documentId] = saved.recoveryKey;
            restoredDocumentIds_[index] = *result.documentId;
            refresh();
            const auto view = editorViews_.find(*result.documentId);
            if (view != editorViews_.end())
                view->second.view->restoreCursorPosition(
                    static_cast<std::size_t>(saved.cursorOffset));
        } else {
            status_->setText(tr("Could not restore %1: %2")
                                 .arg(saved.path, fromUtf8(result.error)));
        }
        restoreSessionDocumentAt(index + 1);
    });
    if (!request) {
        status_->setText(tr("Could not restore %1.")
                             .arg(saved.path));
        restoreSessionDocumentAt(index + 1);
    }
}

void MainWindow::showFind() {
    if (!searchPanel_) return;
    searchPanel_->setActiveView(activeEditorView());
    searchPanel_->openFind();
}

void MainWindow::showReplace() {
    if (!searchPanel_) return;
    searchPanel_->setActiveView(activeEditorView());
    searchPanel_->openReplace();
}

void MainWindow::showSettings() {
    SettingsDialog dialog(settingsManager_, themeManager_, keybindingManager_,
                          [this] { applyCustomisation(); }, this, &commandRegistry_);
    dialog.exec();
}

void MainWindow::applyCustomisation() {
    const auto& value = settingsManager_.settings();
    const auto theme = themeManager_.loadTheme(value.themeName);
    for (const auto& [commandId, action] : commandActions_)
        action->setShortcut(keybindingManager_.binding(commandId));
    QPalette palette;
    palette.setColor(QPalette::Window, theme.window);
    palette.setColor(QPalette::Base, theme.background);
    palette.setColor(QPalette::Text, theme.foreground);
    palette.setColor(QPalette::Highlight, theme.selection);
    palette.setColor(QPalette::HighlightedText, theme.foreground);
    palette.setColor(QPalette::Button, theme.menu);
    palette.setColor(QPalette::ButtonText, theme.foreground);
    setPalette(palette);
    setStyleSheet(QStringLiteral(
        "QMenuBar, QMenu, QToolBar { background-color: %1; color: %2; }"
        "QStatusBar { background-color: %5; color: %2; }"
        "QTabBar::tab { background: %3; color: %2; }"
        "QTabBar::tab:selected { background: %4; }")
        .arg(theme.menu.name(), theme.foreground.name(),
             theme.tab.name(), theme.activeTab.name(), theme.status.name()));
    const std::pair<const char*, bool> presentationCommands[] = {
        {"view.lineNumbers", value.lineNumbers},
        {"view.whitespace", value.whitespaceVisible},
        {"view.indentationGuides", value.indentationGuides},
        {"view.wordWrap", value.wordWrap}
    };
    for (const auto& [id, checked] : presentationCommands) {
        const auto found = commandActions_.find(id);
        if (found != commandActions_.end()) {
            const QSignalBlocker blocker(found->second);
            found->second->setChecked(checked);
        }
    }
    statusBar()->setVisible(value.statusBarVisible);
    menuBar()->setVisible(value.menuVisible);
    if (toolBar_) {
        toolBar_->clear();
        toolBar_->setVisible(value.toolbarVisible);
        for (const auto& id : value.toolbarActions) {
            const auto found = commandActions_.find(id.toStdString());
            if (found != commandActions_.end()) toolBar_->addAction(found->second);
        }
    }

    QFont font(value.fontFamily);
    font.setPointSizeF(value.fontSize * value.editorZoom / 100.0);
    font.setWeight(static_cast<QFont::Weight>(value.fontWeight));
    const auto baseHeight = QFontMetricsF(font).height();
    const int lineHeight = std::max(1, static_cast<int>(std::ceil(
        baseHeight * value.lineSpacing / 100.0 + 2.0)));
    for (auto& binding : editorViews_) {
        auto* view = binding.second.view;
        view->setIndentationConfiguration(
            value.indentationStyle == QStringLiteral("tabs"),
            static_cast<std::size_t>(value.indentationWidth));
        auto configuration = view->renderer()->configuration();
        configuration.font = font;
        configuration.lineHeight = lineHeight;
        configuration.tabWidth = value.indentationWidth;
        configuration.lineNumbers = value.lineNumbers;
        configuration.relativeLineNumbers = value.relativeLineNumbers;
        configuration.currentLineHighlight = value.currentLineHighlight;
        configuration.indentationGuides = value.indentationGuides;
        configuration.showWhitespace = value.whitespaceVisible;
        configuration.matchingOccurrences = value.matchingOccurrences;
        configuration.wordWrap = value.wordWrap;
        configuration.foreground = theme.foreground;
        configuration.background = theme.background;
        configuration.selection = theme.selection;
        configuration.caret = theme.cursor;
        configuration.matchingBracket = theme.matchingBracket;
        configuration.currentLine = theme.currentLine;
        configuration.lineNumber = theme.lineNumber;
        configuration.activeLineNumber = theme.activeLineNumber;
        configuration.indentationGuide = theme.indentationGuide;
        configuration.whitespace = theme.whitespace;
        configuration.matchingOccurrence = theme.matchingOccurrence;
        configuration.searchMatch = theme.searchMatch;
        view->renderer()->setConfiguration(configuration);
        editor::BracketConfiguration brackets;
        brackets.parentheses = value.pairing.contains("round") ? value.pairing.at("round") : true;
        brackets.squareBrackets = value.pairing.contains("square") ? value.pairing.at("square") : true;
        brackets.curlyBraces = value.pairing.contains("curly") ? value.pairing.at("curly") : true;
        brackets.doubleQuotes = value.pairing.contains("doubleQuote") ? value.pairing.at("doubleQuote") : true;
        brackets.singleQuotes = value.pairing.contains("singleQuote") ? value.pairing.at("singleQuote") : true;
        if (!value.automaticPairing) {
            brackets.parentheses = brackets.squareBrackets =
                brackets.curlyBraces = brackets.doubleQuotes =
                brackets.singleQuotes = false;
        }
        view->setBracketConfiguration(brackets);
    }
    if (searchPanel_) searchPanel_->setActiveView(activeEditorView());
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
    for (const auto& [id, action] : commandActions_)
        action->setEnabled(commandRegistry_.isEnabled(id));
    if (searchPanel_) searchPanel_->setActiveView(activeEditorView());

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
        if (!recoveryKeys_.contains(id))
            recoveryKeys_.emplace(
                id, QUuid::createUuid().toString(QUuid::WithoutBraces));
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
        view->setShortcutHandler(
            [this](QKeyEvent* event) { return shortcutDispatcher_.dispatch(event); });
        connect(view, &EditorView::contextMenuRequested, this,
                [this](const QPoint& position) { showContextMenuAt(position); });
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
        recoveryKeys_.erase(it->first);
        it = editorViews_.erase(it);
    }
    applyCustomisation();
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
    if (sessionRecovery_) {
        sessionRecovery_->scheduleSessionSave();
        sessionRecovery_->scheduleRecoverySnapshot();
    }
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
        if (sessionRecovery_) {
            const auto key = recoveryKeys_.find(id);
            if (key != recoveryKeys_.end()) sessionRecovery_->removeRecovery(key->second);
        }
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
        if (sessionRecovery_) {
            const auto key = recoveryKeys_.find(id);
            if (key != recoveryKeys_.end()) sessionRecovery_->removeRecovery(key->second);
        }
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
    const auto recoveryKey = recoveryKeys_.find(id);
    const auto result = workflow_.close(id, choice, std::move(savePath));
    if (result.status == application::OperationStatus::conflict) {
        showSaveConflict(id, fromUtf8(result.error));
    } else if (result.status == application::OperationStatus::failed) {
        showError(tr("Could not close the document: %1").arg(fromUtf8(result.error)));
    }
    if (result && sessionRecovery_ && recoveryKey != recoveryKeys_.end())
        sessionRecovery_->removeRecovery(recoveryKey->second);
    refresh();
}

bool MainWindow::resolveAllDocumentsForWindowClose(
    application::SessionState* resolvedSessionState) {
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

    if (resolvedSessionState)
        *resolvedSessionState = captureSessionState();

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
    if (event->type() == QEvent::KeyPress &&
        shortcutDispatcher_.dispatch(static_cast<QKeyEvent*>(event)))
        return true;
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

void MainWindow::contextMenuEvent(QContextMenuEvent* event) {
    showContextMenuAt(event->globalPos());
}

void MainWindow::showContextMenuAt(const QPoint& globalPosition) {
    // Fixed conventional order; disabled entries stay visible so they are discoverable.
    static constexpr const char* layout[] = {
        "edit.undo", "edit.redo", nullptr, "edit.indent", "edit.outdent", nullptr,
        "search.find", "search.next", "search.previous", "search.replace"};
    QMenu menu(this);
    for (const char* id : layout) {
        if (!id) {
            menu.addSeparator();
            continue;
        }
        const auto action = commandActions_.find(id);
        if (action == commandActions_.end()) continue;
        action->second->setEnabled(commandRegistry_.isEnabled(id));
        menu.addAction(action->second);
    }
    menu.exec(globalPosition);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    auto sessionState = sessionRecovery_
        ? captureSessionState() : application::SessionState{};
    if (closingResolved_ ||
        resolveAllDocumentsForWindowClose(sessionRecovery_ ? &sessionState : nullptr)) {
        if (sessionRecovery_) {
            sessionRecovery_->finishNormally(sessionState,
                                             preserveRecoveryForNextStartup_);
            if (!sessionRecovery_->errorString().isEmpty())
                showError(sessionRecovery_->errorString());
        }
        workflow_.shutdown();
        closingResolved_ = true;
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::preserveSessionOnApplicationQuit() {
    if (closingResolved_ || !sessionRecovery_) return;
    const auto state = captureSessionState();
    sessionRecovery_->flushNow();
    sessionRecovery_->finishNormally(state, true);
    workflow_.shutdown();
    closingResolved_ = true;
}

} // namespace note::ui
