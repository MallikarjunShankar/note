#include "application/document_workflow.hpp"

#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Background external-file inspections can still hold a file open on Windows after the
// workflow is destroyed, so plain QTemporaryDir removal fails silently. Retry until the
// handle closes; runs during exception unwinding too, so failed tests also clean up.
class TestDirectory : public QTemporaryDir {
public:
    ~TestDirectory() {
        if (!isValid()) return;
        QElapsedTimer timer;
        timer.start();
        while (!QDir(path()).removeRecursively() && timer.elapsed() < 5000) QThread::msleep(10);
        if (QDir(path()).exists())
            std::cerr << "warning: could not remove " << path().toStdString() << std::endl;
    }
};

std::filesystem::path pathFromQString(const QString& value) {
#if defined(_WIN32)
    return std::filesystem::path(value.toStdWString());
#else
    const auto bytes = value.toUtf8();
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t*>(bytes.constData()),
        static_cast<std::size_t>(bytes.size())));
#endif
}

void writeFile(const std::filesystem::path& path, const QByteArray& bytes) {
#if defined(_WIN32)
    const auto qpath = QString::fromStdWString(path.native());
#else
    const auto value = path.u8string();
    const auto qpath = QString::fromUtf8(reinterpret_cast<const char*>(value.data()),
                                         static_cast<qsizetype>(value.size()));
#endif
    QFile file(qpath);
    require(file.open(QIODevice::WriteOnly), "could not create test file");
    require(file.write(bytes) == bytes.size(), "could not write test file");
}

bool removeFileWhenAvailable(const QString& path) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        if (!QFile::exists(path) || QFile::remove(path)) return true;
        QEventLoop waitLoop;
        QTimer::singleShot(10, &waitLoop, &QEventLoop::quit);
        waitLoop.exec();
    }
    return !QFile::exists(path);
}

QByteArray readFile(const std::filesystem::path& path) {
#if defined(_WIN32)
    const auto qpath = QString::fromStdWString(path.native());
#else
    const auto value = path.u8string();
    const auto qpath = QString::fromUtf8(reinterpret_cast<const char*>(value.data()),
                                         static_cast<qsizetype>(value.size()));
#endif
    QFile file(qpath);
    require(file.open(QIODevice::ReadOnly), "could not read test file");
    return file.readAll();
}

void testCreateOpenSwitchAndDuplicate() {
    using namespace note::application;
    TestDirectory temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    const auto filePath = pathFromQString(temporary.filePath(QStringLiteral("source.txt")));
    writeFile(filePath, QByteArray("opened text", 11));

    DocumentWorkflow workflow;
    auto& first = workflow.createNew();
    auto& second = workflow.createNew();
    require(first.id() != second.id(), "untitled document identities must differ");
    require(workflow.tabs().size() == 2 && workflow.tabs().at(0) == first.id() &&
            workflow.tabs().at(1) == second.id(), "new documents append in tab order");
    require(workflow.activeDocument() == &second, "new document becomes active");
    require(workflow.activate(first.id()) && workflow.activeDocument() == &first,
            "switching changes active identity without recreating the document");

    const auto opened = workflow.open(filePath);
    require(opened.status == OperationStatus::success && opened.documentId,
            "open registers the file");
    const auto openedId = *opened.documentId;
    auto* openedDocument = workflow.findDocument(openedId);
    require(openedDocument && openedDocument->buffer().text() == "opened text" &&
            workflow.activeDocument() == openedDocument, "opened document active with contents");
    const auto tabCount = workflow.tabs().size();
    require(static_cast<bool>(workflow.activate(first.id())),
            "switch to another document before duplicate open");
    const auto duplicate = workflow.open(filePath);
    require(duplicate.status == OperationStatus::alreadyOpen && duplicate.documentId == openedId,
            "opening an already open path activates its existing document");
    require(workflow.tabs().size() == tabCount && workflow.documentCount() == tabCount,
            "duplicate open must not create another tab or document");

    const auto missingPath = pathFromQString(temporary.filePath(QStringLiteral("missing.txt")));
    const auto badOpen = workflow.open(missingPath);
    require(badOpen.status == OperationStatus::failed && workflow.tabs().size() == tabCount,
            "failed open must not leave a tab or document");
    const auto cancelledOpen = workflow.open({});
    require(cancelledOpen.status == OperationStatus::cancelled && workflow.tabs().size() == tabCount,
            "empty open path represents cancellation without state change");
}

void testSaveAsCollisionAndCloseChoices() {
    using namespace note::application;
    TestDirectory temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    const auto firstPath = pathFromQString(temporary.filePath(QStringLiteral("first.txt")));
    const auto secondPath = pathFromQString(temporary.filePath(QStringLiteral("second.txt")));
    const auto existingTarget = pathFromQString(temporary.filePath(QStringLiteral("existing-target.txt")));
    writeFile(firstPath, QByteArray("one", 3));
    writeFile(secondPath, QByteArray("two", 3));
    writeFile(existingTarget, QByteArray("old target", 10));

    DocumentWorkflow workflow;
    const auto openedFirst = workflow.open(firstPath);
    const auto openedSecond = workflow.open(secondPath);
    require(openedFirst && openedSecond && workflow.tabs().size() == 2,
            "open two documents for save collision checks");
    const auto firstId = *openedFirst.documentId;
    const auto secondId = *openedSecond.documentId;
    auto* first = workflow.findDocument(firstId);
    first->buffer().insert(first->buffer().sizeBytes(), " changed");
    require(first->modified(), "editing marks document modified");

    const auto collision = workflow.saveAs(firstId, secondPath);
    require(collision.status == OperationStatus::failed && first->metadata().path == firstPath &&
            readFile(secondPath) == QByteArray("two", 3),
            "Save As collision leaves both documents and destination unchanged");
    require(workflow.saveAs(firstId, {}) .status == OperationStatus::cancelled && first->modified(),
            "cancelled Save As leaves modified document unchanged");

    const auto cancelledClose = workflow.close(firstId, CloseChoice::cancel);
    require(cancelledClose.status == OperationStatus::cancelled && workflow.findDocument(firstId) == first &&
            workflow.tabs().contains(firstId), "cancel close preserves document and tab");

    const auto existingInspection = workflow.inspectFile(existingTarget);
    require(existingInspection.exists && existingInspection.metadata.hasFingerprint,
            "existing Save As target can be fingerprinted for explicit replacement");
    const auto replaceExisting = workflow.saveAs(firstId, existingTarget, true,
                                                 existingInspection.metadata.sha256);
    require(replaceExisting.status == OperationStatus::success && !first->modified() &&
            first->metadata().path == existingTarget && readFile(existingTarget) == QByteArray("one changed", 11),
            "Save As atomically replaces an existing destination and updates association after success");
    first->buffer().insert(first->buffer().sizeBytes(), " again");
    const auto closeAfterCancel = workflow.close(firstId, CloseChoice::cancel);
    require(closeAfterCancel.status == OperationStatus::cancelled && workflow.tabs().contains(firstId),
            "cancel close still preserves document after a successful Save As");
    const auto closeAfterSave = workflow.close(firstId, CloseChoice::save);
    require(closeAfterSave.status == OperationStatus::success && !workflow.findDocument(firstId) &&
            !workflow.tabs().contains(firstId) && readFile(existingTarget) == QByteArray("one changed again", 17),
            "successful save closes the document and persists its content");
    require(workflow.activeDocument() && workflow.activeDocument()->id() == secondId,
            "closing active tab selects its remaining neighbor");

    auto* second = workflow.findDocument(secondId);
    second->buffer().insert(0, "edited ");
    const auto discarded = workflow.close(secondId, CloseChoice::discard);
    require(discarded.status == OperationStatus::success && workflow.documentCount() == 0 &&
            workflow.tabs().size() == 0 && !workflow.activeDocument(),
            "discard closes final tab and clears active document");
    require(readFile(secondPath) == QByteArray("two", 3), "discard does not persist unsaved text");
}

void testSaveFailureOnCloseAndUntitledSave() {
    using namespace note::application;
    TestDirectory temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    const auto path = pathFromQString(temporary.filePath(QStringLiteral("replace-me.txt")));
    writeFile(path, QByteArray("original", 8));

    DocumentWorkflow workflow;
    const auto opened = workflow.open(path);
    require(static_cast<bool>(opened), "open document for failed save close");
    const auto id = *opened.documentId;
    auto* document = workflow.findDocument(id);
    document->buffer().insert(document->buffer().sizeBytes(), " modified");
    std::filesystem::remove(path);
    std::filesystem::create_directory(path);
    const auto failed = workflow.close(id, CloseChoice::save);
    require(failed.status == OperationStatus::failed && workflow.findDocument(id) == document &&
            document->modified() && workflow.tabs().contains(id),
            "save failure during close leaves dirty document and tab intact");

    auto& untitled = workflow.createNew();
    untitled.buffer().insert(0, "new document");
    const auto noPath = workflow.close(untitled.id(), CloseChoice::save);
    require(noPath.status == OperationStatus::needsSavePath && workflow.tabs().contains(untitled.id()),
            "untitled Save choice asks caller for Save As path");
    const auto cancelSaveAs = workflow.close(untitled.id(), CloseChoice::save, std::filesystem::path{});
    require(cancelSaveAs.status == OperationStatus::cancelled && workflow.tabs().contains(untitled.id()) &&
            untitled.modified(), "cancelled untitled Save As keeps document open");
    const auto savePath = pathFromQString(temporary.filePath(QStringLiteral("new.txt")));
    const auto closeAfterSave = workflow.close(untitled.id(), CloseChoice::save, savePath);
    require(closeAfterSave.status == OperationStatus::success && readFile(savePath) == QByteArray("new document", 12),
            "untitled document is saved before closing");

    const auto discard = workflow.close(id, CloseChoice::discard);
    require(discard.status == OperationStatus::success && workflow.documentCount() == 0,
            "discard after save failure safely closes modified document");
}

void testAsynchronousProgressiveOpen() {
    using namespace note::application;
    TestDirectory temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    const auto path = pathFromQString(temporary.filePath(QStringLiteral("large.txt")));
    writeFile(path, QByteArray(8 * 1024 * 1024, 'x'));
    DocumentWorkflow workflow;
    bool completed = false;
    bool duplicateCompleted = false;
    WorkflowResult result;
    std::size_t progressEvents = 0;
    QEventLoop loop;
    const auto request = workflow.openAsync(path,
        [&](auto, note::documents::OpenProgress update) { if (!update.buildingIndex) ++progressEvents; },
        [&](auto, WorkflowResult value) { completed = true; result = std::move(value); loop.quit(); });
    require(request != 0 && workflow.documentCount() == 0 && workflow.tabs().size() == 0,
            "pending open is not published as a document or tab");
    const auto duplicate = workflow.openAsync(path, {},
        [&](auto, WorkflowResult value) { duplicateCompleted = value.status == OperationStatus::success; });
    require(duplicate == request, "duplicate pending open shares the existing request");
    bool timedOut = false;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { timedOut = true; loop.quit(); });
    timeout.start(15000);
    while (!completed && !timedOut) loop.exec();
    require(!timedOut, "background open completes before test timeout");
    require(result.status == OperationStatus::success && result.documentId &&
            workflow.documentCount() == 1 && workflow.tabs().size() == 1,
            "completed background open is published atomically");
    require(duplicateCompleted, "duplicate pending caller receives the shared completion");
    require(progressEvents > 0, "asynchronous opening reports reading progress");

    const auto cancelPath = pathFromQString(temporary.filePath(QStringLiteral("cancel.txt")));
    writeFile(cancelPath, QByteArray(8 * 1024 * 1024, 'y'));
    completed = false;
    const auto cancelRequest = workflow.openAsync(cancelPath, {},
        [&](auto, WorkflowResult value) { completed = true; result = std::move(value); loop.quit(); });
    workflow.cancelOpen(cancelRequest);
    timedOut = false;
    timeout.start(15000);
    while (!completed && !timedOut) loop.exec();
    require(!timedOut, "cancelled background open completes before test timeout");
    require(result.status == OperationStatus::cancelled && workflow.documentCount() == 1 &&
            workflow.tabs().size() == 1,
            "cancelled background open leaves no partial document or tab");
    const auto shutdownPath = pathFromQString(temporary.filePath(QStringLiteral("shutdown.txt")));
    writeFile(shutdownPath, QByteArray(8 * 1024 * 1024, 'z'));
    bool callbackAfterShutdown = false;
    (void)workflow.openAsync(shutdownPath, {},
        [&](auto, WorkflowResult) { callbackAfterShutdown = true; });
    workflow.shutdown();
    QEventLoop shutdownLoop;
    QTimer::singleShot(100, &shutdownLoop, &QEventLoop::quit);
    shutdownLoop.exec();
    require(!callbackAfterShutdown && workflow.documentCount() == 1 && workflow.tabs().size() == 1,
            "shutdown cancels pending work and suppresses callbacks and partial publication");
}

void testExternalModificationConflictAndReload() {
    using namespace note::application;
    using note::documents::ExternalFileState;
    TestDirectory temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    const auto path =
        pathFromQString(temporary.filePath(QStringLiteral("external.txt")));
    writeFile(path, QByteArray("original\n", 9));

    DocumentWorkflow workflow;
    const auto opened = workflow.open(path);
    require(opened && opened.documentId, "open document for external-change checks");
    const auto id = *opened.documentId;
    auto* document = workflow.findDocument(id);
    document->buffer().insert(document->buffer().sizeBytes(), " local");

    bool changedReported = false;
    bool missingReported = false;
    workflow.setExternalFileChangeCallback(
        [&](note::documents::DocumentId changedId, ExternalFileState state) {
            if (changedId != id) return;
            changedReported = changedReported || state == ExternalFileState::changed;
            missingReported = missingReported || state == ExternalFileState::missing;
        });

    const QByteArray externalContents("external\r\n", 10);
    writeFile(path, externalContents);
    workflow.checkExternalChanges();
    QEventLoop inspectionLoop;
    QTimer inspectionTimeout;
    inspectionTimeout.setSingleShot(true);
    QObject::connect(&inspectionTimeout, &QTimer::timeout, &inspectionLoop,
                     &QEventLoop::quit);
    inspectionTimeout.start(10000);
    while (!changedReported && inspectionTimeout.isActive())
        inspectionLoop.exec();
    require(changedReported, "external content changes are reported");

    const auto save = workflow.save(id);
    require(save.status == OperationStatus::conflict && document->modified() &&
                readFile(path) == externalContents,
            "save conflict preserves local edits and external file contents");
    require(workflow.reloadAsync(id, false, {}, {}) == 0,
            "reload refuses to discard local edits without confirmation");

    bool reloadCompleted = false;
    WorkflowResult reloadResult;
    QEventLoop reloadLoop;
    const auto generation = document->contentGeneration();
    const auto reloadRequest = workflow.reloadAsync(
        id, true, {},
        [&](auto, WorkflowResult result) {
            reloadResult = std::move(result);
            reloadCompleted = true;
            reloadLoop.quit();
        });
    require(reloadRequest != 0, "confirmed reload request is accepted");
    QTimer reloadTimeout;
    reloadTimeout.setSingleShot(true);
    QObject::connect(&reloadTimeout, &QTimer::timeout, &reloadLoop,
                     &QEventLoop::quit);
    reloadTimeout.start(10000);
    while (!reloadCompleted && reloadTimeout.isActive()) reloadLoop.exec();
    require(reloadCompleted && reloadResult.status == OperationStatus::success &&
                document->buffer().text() == "external\n" && !document->modified() &&
                document->contentGeneration() == generation + 1,
            "confirmed reload replaces contents without changing document identity");

    require(removeFileWhenAvailable(temporary.filePath(QStringLiteral("external.txt"))),
            "could not remove the test file before checking missing-file detection");
    workflow.checkExternalChanges();
    QEventLoop missingLoop;
    QTimer missingTimeout;
    missingTimeout.setSingleShot(true);
    QObject::connect(&missingTimeout, &QTimer::timeout, &missingLoop,
                     &QEventLoop::quit);
    missingTimeout.start(10000);
    while (!missingReported && missingTimeout.isActive()) missingLoop.exec();
    require(missingReported, "external file removal is reported");
    const auto saveMissing = workflow.save(id);
    require(saveMissing.status == OperationStatus::conflict &&
                !document->modified() &&
                document->buffer().text() == "external\n",
            "saving a missing file is rejected without changing document state");
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        testCreateOpenSwitchAndDuplicate();
        testSaveAsCollisionAndCloseChoices();
        testSaveFailureOnCloseAndUntitledSave();
        testExternalModificationConflictAndReload();
        testAsynchronousProgressiveOpen();
        std::cout << "All document workflow tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Document workflow test failure: " << error.what() << '\n';
        return 1;
    }
}
