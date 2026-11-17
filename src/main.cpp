#include "application/document_workflow.hpp"
#include "ui/main_window.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    note::application::DocumentWorkflow workflow;
    note::ui::MainWindow window(workflow);
    window.show();
    return application.exec();
}
