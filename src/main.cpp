#include "application/document_workflow.hpp"
#include "ui/main_window.hpp"

#include <QApplication>
#include <QStandardPaths>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    note::application::DocumentWorkflow workflow;
    note::application::SessionRecoveryService session(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const auto previousSession = session.start();
    note::ui::MainWindow window(workflow, &session, previousSession);
    window.show();
    return application.exec();
}
