#include "application/document_workflow.hpp"
#include "application/session_recovery.hpp"
#include "platform/path_resolver.hpp"
#include "ui/editor_view.hpp"
#include "ui/main_window.hpp"

#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>

#include <stdexcept>
#include <optional>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void writeFile(const QString& path, const QByteArray& contents) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "fixture file could not be opened");
    require(file.write(contents) == contents.size(), "fixture file could not be written");
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    try {
        QTemporaryDir temporary;
        require(temporary.isValid(), "temporary directory unavailable");
        const auto firstPath = temporary.filePath("first.txt");
        const auto secondPath = temporary.filePath("second.txt");
        const auto missingPath = temporary.filePath("moved.txt");
        const auto dataPath = temporary.filePath("app-data");
        writeFile(firstPath, "first document");
        writeFile(secondPath, "second document");

        note::application::SessionState prior;
        prior.activeIndex = 1;
        prior.documents = {
            {firstPath, QStringLiteral("session-one"), 2},
            {secondPath, QStringLiteral("session-two"), 4},
            {missingPath, QStringLiteral("session-missing"), 0}};
        {
            note::application::SessionRecoveryService seed(dataPath);
            (void)seed.start();
            seed.setProviders([&prior] { return prior; }, [] {
                return std::vector<note::application::RecoveryDocument>{};
            });
            seed.flushNow();
        }

        note::application::SessionRecoveryService session(dataPath);
        const auto startup = session.start();
        require(!startup.previousRunClean && startup.documents.size() == 3,
                "previous interrupted session is available");
        note::application::DocumentWorkflow workflow;
        note::ui::MainWindow window(workflow, &session, startup);
        window.show();

        auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabs"));
        require(tabs != nullptr, "document tabs are available");
        QEventLoop wait;
        QTimer poll;
        poll.setInterval(10);
        QObject::connect(&poll, &QTimer::timeout, &wait, [&] {
            if (tabs->count() == 2) wait.quit();
        });
        QTimer::singleShot(3000, &wait, &QEventLoop::quit);
        poll.start();
        wait.exec();
        require(tabs->count() == 2 && workflow.documentCount() == 2,
                "available documents restore and missing paths are skipped");
        const auto* active = workflow.activeDocument();
        require(active && active->metadata().path &&
                    *active->metadata().path ==
                        note::platform::pathFromQString(secondPath),
                "saved active document is restored");
        auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("editorStack"));
        auto* view = stack ? qobject_cast<note::ui::EditorView*>(stack->currentWidget())
                           : nullptr;
        require(view && view->cursors().primaryOffset() == 4,
                "active document cursor position is restored");

        window.close();
        require(session.errorString().isEmpty(),
                "normal window shutdown persists clean session state");
        workflow.shutdown();
        note::application::SessionRecoveryService verify(dataPath);
        const auto clean = verify.start();
        require(clean.previousRunClean && clean.documents.size() == 2,
                "normal shutdown records the restored document session");
        verify.finishNormally(clean);

        note::application::SessionState interrupted;
        interrupted.activeIndex = 0;
        interrupted.documents = {
            {firstPath, QStringLiteral("recovery-source"), 4}};
        note::editor::TextBuffer recoveredText("unsaved recovery version");
        recoveredText.insert(0, "!");
        {
            note::application::SessionRecoveryService seed(dataPath);
            (void)seed.start();
            seed.setProviders([&interrupted] { return interrupted; },
                [&] {
                    return std::vector<note::application::RecoveryDocument>{
                        {"recovery-source", firstPath, &recoveredText,
                         recoveredText.revision()}};
                });
            seed.flushNow();
        }

        note::application::SessionRecoveryService recoverySession(dataPath);
        const auto recoveryStartup = recoverySession.start();
        require(!recoveryStartup.previousRunClean,
                "recovery test starts from an interrupted session");
        note::application::DocumentWorkflow recoveryWorkflow;
        note::ui::MainWindow recoveryWindow(recoveryWorkflow, &recoverySession,
                                            recoveryStartup);
        QTimer::singleShot(100, &app, [] {
            auto* prompt = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (!prompt) return;
            for (auto* button : prompt->findChildren<QPushButton*>()) {
                if (button->text().contains("Recover as Untitled")) {
                    button->click();
                    return;
                }
            }
        });
        recoveryWindow.show();
        QEventLoop recoveryWait;
        QTimer recoveryPoll;
        recoveryPoll.setInterval(10);
        QObject::connect(&recoveryPoll, &QTimer::timeout, &recoveryWait, [&] {
            if (recoveryWorkflow.documentCount() == 2) recoveryWait.quit();
        });
        QTimer::singleShot(3000, &recoveryWait, &QEventLoop::quit);
        recoveryPoll.start();
        recoveryWait.exec();
        require(recoveryWorkflow.documentCount() == 2,
                "recovery creates a separate untitled document and restores source");
        bool foundRecovered = false;
        std::optional<note::documents::DocumentId> recoveredId;
        for (const auto id : recoveryWorkflow.tabs().order()) {
            const auto* document = recoveryWorkflow.findDocument(id);
            if (!document || document->metadata().path) continue;
            foundRecovered = document->buffer().text() ==
                             "!unsaved recovery version";
            recoveredId = id;
        }
        QFile sourceAfterRecovery(firstPath);
        require(foundRecovered && recoveredId &&
                    sourceAfterRecovery.open(QIODevice::ReadOnly) &&
                    sourceAfterRecovery.readAll() == "first document",
                "recovery keeps unsaved content separate from unchanged original");
        (void)recoveryWorkflow.close(*recoveredId,
                                     note::application::CloseChoice::discard);
        recoveryWindow.refreshFromModel();
        recoveryWindow.close();
        require(recoverySession.errorString().isEmpty(),
                "recovered session shuts down normally");

        const auto quitDataPath = temporary.filePath("quit-app-data");
        note::application::SessionRecoveryService quitSession(quitDataPath);
        const auto quitStartup = quitSession.start();
        note::application::DocumentWorkflow quitWorkflow;
        auto& quitDocument = quitWorkflow.createNew();
        note::ui::MainWindow quitWindow(quitWorkflow, &quitSession, quitStartup);
        quitWindow.show();
        quitDocument.buffer().insert(0, "saved before QApplication::quit");
        quitWindow.refreshFromModel();
        quitWindow.preserveSessionOnApplicationQuit();
        note::application::SessionRecoveryService quitVerification(quitDataPath);
        const auto quitState = quitVerification.start();
        const auto quitSnapshots = quitVerification.recoveries();
        require(!quitState.previousRunClean && quitSnapshots.size() == 1 &&
                    quitSnapshots.front().contents &&
                    quitSnapshots.front().contents->utf8 ==
                        "saved before QApplication::quit",
                "application quit outside window close preserves a recoverable session");
        quitVerification.finishNormally(quitState);
    } catch (const std::exception& error) {
        qCritical("Session lifecycle smoke failure: %s", error.what());
        return 1;
    }
    return 0;
}
