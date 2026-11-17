#include "application/document_workflow.hpp"
#include "ui/main_window.hpp"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QMetaObject>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTabBar>
#include <QTimer>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    try {
        note::application::DocumentWorkflow workflow;
        note::ui::MainWindow window(workflow);
        window.show();
        auto* newAction = window.findChild<QAction*>(QStringLiteral("newDocumentAction"));
        auto* closeAction = window.findChild<QAction*>(QStringLiteral("closeDocumentAction"));
        auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabs"));
        auto* preview = window.findChild<QPlainTextEdit*>(QStringLiteral("documentPreview"));
        require(newAction && closeAction && tabs && preview, "main window exposes file actions, tabs, and preview");
        require(preview->isReadOnly(), "document preview cannot modify text");
        require(tabs->count() == 0 && !closeAction->isEnabled(), "empty workspace state");

        newAction->trigger();
        newAction->trigger();
        require(tabs->count() == 2 && workflow.tabs().size() == 2,
                "New action creates two distinct tabs");
        require(workflow.activeDocument() &&
                workflow.activeDocument()->id() == *workflow.tabs().at(1),
                "new tab becomes active");

        tabs->setCurrentIndex(0);
        require(workflow.activeDocument() &&
                workflow.activeDocument()->id() == *workflow.tabs().at(0),
                "tab selection activates its associated document");
        auto* active = workflow.activeDocument();
        active->buffer().insert(0, "unsaved");
        window.refreshFromModel();
        require(tabs->tabText(0).endsWith(QLatin1Char('*')) && window.windowTitle().contains(QLatin1Char('*')),
                "modified marker is reflected in tab and window title");
        require(preview->toPlainText() == QStringLiteral("unsaved"),
                "preview reflects the active model contents");
        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Cancel)) prompt->button(QMessageBox::Cancel)->click();
        });
        require(QMetaObject::invokeMethod(tabs, "tabCloseRequested", Qt::DirectConnection,
                                          Q_ARG(int, 0)), "dirty tab close prompt dispatched");
        require(workflow.findDocument(active->id()) && tabs->count() == 2,
                "cancel close prompt keeps dirty tab open");

        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Discard)) prompt->button(QMessageBox::Discard)->click();
        });
        require(QMetaObject::invokeMethod(tabs, "tabCloseRequested", Qt::DirectConnection,
                                          Q_ARG(int, 0)), "discard tab close prompt dispatched");
        window.refreshFromModel();
        require(tabs->count() == 1 && workflow.activeDocument() &&
                workflow.activeDocument()->id() == *workflow.tabs().at(0),
                "discard closes dirty tab and selects its neighbor");

        closeAction->trigger();
        require(tabs->count() == 0 && workflow.documentCount() == 0 &&
                !workflow.activeDocument() && !closeAction->isEnabled(),
                "closing final tab leaves a valid empty workspace");

        newAction->trigger();
        workflow.activeDocument()->buffer().insert(0, "unsaved window document");
        window.refreshFromModel();
        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Cancel)) prompt->button(QMessageBox::Cancel)->click();
        });
        require(!window.close() && workflow.documentCount() == 1,
                "cancelling window close retains all documents");
        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Discard)) prompt->button(QMessageBox::Discard)->click();
        });
        require(window.close() && workflow.documentCount() == 0,
                "discard confirmation closes the complete workspace");
        std::cout << "Main window offscreen smoke test passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Main window smoke failure: " << error.what() << '\n';
        return 1;
    }
}
