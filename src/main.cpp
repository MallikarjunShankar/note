#include "application/document_workflow.hpp"
#include "ui/main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QStandardPaths>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Note"));
    note::application::DocumentWorkflow workflow;
    note::application::SessionRecoveryService session(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const auto previousSession = session.start();
    note::ui::MainWindow window(workflow, &session, previousSession);
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &window,
                     [&window] { window.preserveSessionOnApplicationQuit(); });
    window.show();
    return application.exec();
}
