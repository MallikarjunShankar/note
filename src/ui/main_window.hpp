#pragma once

#include "application/document_workflow.hpp"
#include "commands/command_registry.hpp"
#include "commands/keybinding_manager.hpp"
#include "commands/shortcut_dispatcher.hpp"
#include "customisation/settings_manager.hpp"
#include "customisation/theme_manager.hpp"

#include <QMainWindow>
#include <cstdint>
#include <map>
#include <memory>

class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QAction;
class QTabBar;
class QLabel;
class QProgressDialog;
class QStackedWidget;
class QToolBar;
class QContextMenuEvent;

namespace note::ui {

class EditorView;
class SearchPanel;
class SettingsDialog;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(application::DocumentWorkflow& workflow, QWidget* parent = nullptr);
    ~MainWindow() override;
    // Called after model changes made outside the active editor.
    void refreshFromModel();
    void openPath(const std::filesystem::path& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool event(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void createActions();
    void refresh();
    void refreshPresentation();
    void syncEditorViews();
    void refreshAfterEdit(documents::DocumentId id);
    void openFile();
    void saveActive();
    void saveActiveAs();
    void closeDocument(documents::DocumentId id);
    bool resolveAllDocumentsForWindowClose();
    void showError(const QString& message);
    void startReload(documents::DocumentId id, bool confirmDiscardLocalChanges);
    void showExternalFileChange(documents::DocumentId id, documents::ExternalFileState state);
    void showSaveConflict(documents::DocumentId id, const QString& message);
    void saveDocumentAs(documents::DocumentId id);
    void showFind();
    void showReplace();
    void showSettings();
    void applyCustomisation();
    EditorView* activeEditorView();
    void createOpenProgress(application::DocumentWorkflow::OpenRequestId id, const QString& label);
    void updateOpenProgress(application::DocumentWorkflow::OpenRequestId id, documents::OpenProgress progress);
    void finishOpenProgress(application::DocumentWorkflow::OpenRequestId id);
    [[nodiscard]] QString displayName(const documents::Document& document) const;
    [[nodiscard]] std::optional<std::filesystem::path> requestSavePath(
        const documents::Document& document);

    application::DocumentWorkflow& workflow_;
    QTabBar* tabs_{};
    QStackedWidget* editorStack_{};
    QWidget* emptyEditor_{};
    QLabel* status_{};
    QToolBar* toolBar_{};
    QAction* saveAction_{};
    QAction* saveAsAction_{};
    QAction* closeAction_{};
    QAction* externalOpenAction_{};
    QAction* terminalAction_{};
    bool closingResolved_{};
    std::map<application::DocumentWorkflow::OpenRequestId, QProgressDialog*> openProgress_;
    struct EditorBinding {
        std::uint64_t contentGeneration{};
        EditorView* view{};
    };
    std::map<documents::DocumentId, EditorBinding> editorViews_;
    commands::CommandRegistry commandRegistry_;
    customisation::SettingsManager settingsManager_;
    customisation::ThemeManager themeManager_;
    commands::KeybindingManager keybindingManager_;
    commands::ShortcutDispatcher shortcutDispatcher_;
    SearchPanel* searchPanel_{};
    std::map<std::string, QAction*> commandActions_;
};

} // namespace note::ui
