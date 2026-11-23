// Crash injection: a child process is killed (QProcess::kill, TerminateProcess on
// Windows) while editing or saving; the parent then checks what survived on disk.
#include "application/document_workflow.hpp"
#include "application/session_recovery.hpp"
#include "ui/editor_view.hpp"
#include "ui/main_window.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <cstdio>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void writeFile(const QString& path, const QByteArray& contents) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size(),
            "fixture file could not be written");
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "file could not be read");
    return file.readAll();
}

template <typename Condition>
bool waitFor(Condition condition, int milliseconds) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > milliseconds) return false;
        QApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return true;
}

note::ui::EditorView* activeView(note::ui::MainWindow& window) {
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("editorStack"));
    return stack ? qobject_cast<note::ui::EditorView*>(stack->currentWidget()) : nullptr;
}

void announce(const char* line) {
    std::fputs(line, stdout);
    std::fputs("\n", stdout);
    std::fflush(stdout);
}

// Child: open a file, type unsaved text, wait for the recovery snapshot and the
// session file, then idle until killed.
int editChild(const QString& dataPath, const QString& filePath) {
    note::application::SessionRecoveryService session(dataPath);
    const auto startup = session.start();
    note::application::DocumentWorkflow workflow;
    note::ui::MainWindow window(workflow, &session, startup);
    window.show();
    window.openPath(filePath.toStdWString());
    note::ui::EditorView* view = nullptr;
    require(waitFor([&] { return (view = activeView(window)) != nullptr &&
                                 workflow.activeDocument(); }, 10000),
            "child opened the file");
    for (const QChar character : QStringLiteral("UNSAVED ")) {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(character));
        QApplication::sendEvent(view->renderer(), &event);
    }
    const QDir data(dataPath);
    require(waitFor([&] {
                return !data.entryList({"*.recovery"}, QDir::Files).isEmpty() &&
                       data.exists("session.json") &&
                       readFile(data.filePath("session.json")).contains("original.txt");
            }, 10000),
            "child wrote a recovery snapshot and session");
    announce("READY");
    waitFor([] { return false; }, 60000);
    return 0;
}

// Child: save the same file over and over with two different contents.
int saveChild(const QString& filePath) {
    note::application::DocumentWorkflow workflow;
    const auto opened = workflow.open(filePath.toStdWString());
    require(static_cast<bool>(opened) && opened.documentId, "save child opened the file");
    auto* document = workflow.findDocument(*opened.documentId);
    const std::string versions[] = {std::string(6u << 20, 'A'), std::string(4u << 20, 'B')};
    // Start with the version that differs from the file, so every save changes it.
    const int first = document->buffer().byteAt(0) == 'A' ? 1 : 0;
    announce("SAVING");
    for (int round = first;; ++round) {
        auto& buffer = document->buffer();
        buffer.replace(0, buffer.sizeBytes(), versions[round % 2]);
        require(static_cast<bool>(workflow.save(*opened.documentId)), "save child saved");
    }
}

QProcess* startChild(const QStringList& arguments) {
    auto* child = new QProcess;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM", "offscreen");
    child->setProcessEnvironment(environment);
    child->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    child->start(QCoreApplication::applicationFilePath(), arguments);
    require(child->waitForStarted(10000), "child process started");
    return child;
}

void waitForLine(QProcess& child, const QByteArray& line) {
    QElapsedTimer timer;
    timer.start();
    QByteArray output;
    while (!output.contains(line)) {
        require(timer.elapsed() < 30000 && child.state() == QProcess::Running,
                "child reached its kill point");
        child.waitForReadyRead(100);
        output += child.readAllStandardOutput();
    }
}

void kill(QProcess* child) {
    child->kill();
    require(child->waitForFinished(10000), "killed child exited");
    require(child->exitStatus() == QProcess::CrashExit || child->exitCode() != 0,
            "child was terminated, not shut down");
    delete child;
}

void testKilledEditorRecovers(const QTemporaryDir& temporary) {
    const auto dataPath = temporary.filePath("crash-data");
    const auto filePath = temporary.filePath("original.txt");
    writeFile(filePath, "saved text");
    auto* child = startChild({"--edit-child", dataPath, filePath});
    waitForLine(*child, "READY");
    kill(child);

    require(readFile(filePath) == "saved text", "a crash never touches the original file");
    {
        note::application::SessionRecoveryService verify(dataPath);
        const auto state = verify.start();
        require(!state.previousRunClean && state.documents.size() == 1 &&
                    QFileInfo(state.documents.front().path) == QFileInfo(filePath),
                "the killed run is detected as unclean with its session intact");
        const auto snapshots = verify.recoveries();
        require(snapshots.size() == 1 && snapshots.front().contents &&
                    snapshots.front().contents->utf8 == "UNSAVED saved text",
                "the unsaved edit survives the kill in a recovery snapshot");
    }

    note::application::SessionRecoveryService session(dataPath);
    const auto startup = session.start();
    note::application::DocumentWorkflow workflow;
    note::ui::MainWindow window(workflow, &session, startup);
    QTimer clicker;
    clicker.setInterval(50);
    QObject::connect(&clicker, &QTimer::timeout, [] {
        auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!prompt) return;
        for (auto* button : prompt->findChildren<QPushButton*>())
            if (button->text().contains("Recover as Untitled")) button->click();
    });
    clicker.start();
    window.show();
    require(waitFor([&] { return workflow.documentCount() == 2; }, 10000),
            "after the kill, startup restores the file and the recovered copy");
    bool recovered = false;
    for (const auto id : workflow.tabs().order()) {
        const auto* document = workflow.findDocument(id);
        if (document && !document->metadata().path)
            recovered = document->buffer().text() == "UNSAVED saved text";
    }
    require(recovered && readFile(filePath) == "saved text",
            "recovered text opens as untitled and the original is unchanged");
    clicker.stop();
    for (const auto id : workflow.tabs().order())
        (void)workflow.close(id, note::application::CloseChoice::discard);
    window.refreshFromModel();
    window.close();
    workflow.shutdown();
}

void testKilledSaveIsAtomic(const QTemporaryDir& temporary) {
    const auto filePath = temporary.filePath("saving.txt");
    const QByteArray original(5u << 20, 'O');
    writeFile(filePath, original);
    for (int round = 0; round < 10; ++round) {
        auto* child = startChild({"--save-child", filePath});
        waitForLine(*child, "SAVING");
        QThread::msleep(50 + QRandomGenerator::global()->bounded(1500));
        kill(child);
        const auto contents = readFile(filePath);
        const bool intact = contents == original ||
                            contents == QByteArray(6u << 20, 'A') ||
                            contents == QByteArray(4u << 20, 'B');
        require(intact, "a save killed at any point leaves a complete old or new file");
        // Known limit: a kill mid-write leaves QSaveFile's "saving.txt.XXXXXX" temp.
    }
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    try {
        const auto arguments = QCoreApplication::arguments();
        if (arguments.size() == 4 && arguments[1] == "--edit-child")
            return editChild(arguments[2], arguments[3]);
        if (arguments.size() == 3 && arguments[1] == "--save-child")
            return saveChild(arguments[2]);
        QTemporaryDir temporary;
        require(temporary.isValid(), "temporary directory unavailable");
        testKilledEditorRecovers(temporary);
        testKilledSaveIsAtomic(temporary);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Crash recovery failure: %s\n", error.what());
        return 1;
    }
    std::puts("Crash recovery tests passed.");
    return 0;
}
