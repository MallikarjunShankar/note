#pragma once

#include "application/document_workflow.hpp"

#include <QMainWindow>
#include <map>

class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QAction;
class QTabBar;
class QLabel;
class QProgressDialog;

namespace note::ui {

class DocumentPreview;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(application::DocumentWorkflow& workflow, QWidget* parent = nullptr);
    ~MainWindow() override;
    // Called by model-changing workflows and, once editing is integrated, by
    // the Editor View after a committed edit.
    void refreshFromModel();
    void openPath(const std::filesystem::path& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool event(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void createActions();
    void refresh();
    void refreshPresentation();
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
    void createOpenProgress(application::DocumentWorkflow::OpenRequestId id, const QString& label);
    void updateOpenProgress(application::DocumentWorkflow::OpenRequestId id, documents::OpenProgress progress);
    void finishOpenProgress(application::DocumentWorkflow::OpenRequestId id);
    [[nodiscard]] QString displayName(const documents::Document& document) const;
    [[nodiscard]] std::optional<std::filesystem::path> requestSavePath(
        const documents::Document& document);

    application::DocumentWorkflow& workflow_;
    QTabBar* tabs_{};
    DocumentPreview* preview_{};
    QLabel* status_{};
    QAction* saveAction_{};
    QAction* saveAsAction_{};
    QAction* closeAction_{};
    QAction* externalOpenAction_{};
    QAction* terminalAction_{};
    bool closingResolved_{};
    std::map<application::DocumentWorkflow::OpenRequestId, QProgressDialog*> openProgress_;
};

} // namespace note::ui
