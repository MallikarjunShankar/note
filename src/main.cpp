#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Note"));
    window.resize(1000, 700);
    window.show();
    return application.exec();
}
