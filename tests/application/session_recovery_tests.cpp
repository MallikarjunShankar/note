#include "application/session_recovery.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    const auto directory = temporary.path();

    note::application::SessionState saved;
    saved.activeIndex = 1;
    saved.documents = {
        {QStringLiteral("C:/missing/one.txt"), QStringLiteral("one"), 3},
        {QStringLiteral("C:/missing/two.txt"), QStringLiteral("two"), 8},
        {{}, QStringLiteral("untitled"), 1}};
    note::editor::TextBuffer first("first\nline");
    first.replace(0, 0, "recovered first\n");
    note::editor::TextBuffer second(std::string(2 * 1024 * 1024, 'x'));
    second.replace(10, 4, "large");
    const auto originalPath = QDir(directory).filePath("original.txt");
    QFile original(originalPath);
    require(original.open(QIODevice::WriteOnly), "original fixture open failed");
    require(original.write("original") == 8, "original fixture write failed");
    original.close();

    {
        note::application::SessionRecoveryService service(directory);
        const auto initial = service.start();
        require(initial.previousRunClean && initial.documents.empty(),
                "first start has no prior session");
        service.setProviders([&saved] { return saved; },
            [&] {
                return std::vector<note::application::RecoveryDocument>{
                    {"one", originalPath, &first, first.revision()},
                    {"two", "C:/missing/two.txt", &second, second.revision()}};
            });
        service.flushNow();
        require(service.errorString().isEmpty(),
                "session and recovery snapshot writes succeed");
    }

    {
        note::application::SessionRecoveryService service(directory);
        const auto crashed = service.start();
        require(!crashed.previousRunClean,
                "unclean session is detected on next startup");
        require(crashed.documents.size() == 3 && crashed.activeIndex == 1,
                "tab order and active index are restored");
        QFile externalChange(originalPath);
        require(externalChange.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "external modification fixture open failed");
        require(externalChange.write("external") == 8,
                "external modification fixture write failed");
        externalChange.close();
        const auto snapshots = service.recoveries();
        require(snapshots.size() == 2 && snapshots[0].contents &&
                    snapshots[1].contents,
                "multiple intact snapshots load");
        bool foundFirst = false;
        bool foundLarge = false;
        for (const auto& snapshot : snapshots) {
            if (snapshot.key == "one")
                foundFirst = snapshot.contents->utf8 ==
                    "recovered first\nfirst\nline";
            if (snapshot.key == "two")
                foundLarge = snapshot.contents->utf8 == second.text();
        }
        require(foundFirst && foundLarge,
                "snapshot contents including large documents are preserved");
        service.removeRecovery("one");
        require(service.recoveries().size() == 1,
                "recovery removal prevents repeated restore");
        QFile verifyOriginal(originalPath);
        require(verifyOriginal.open(QIODevice::ReadOnly) &&
                    verifyOriginal.readAll() == "external",
                "recovery snapshots never overwrite the original file");
        service.finishNormally(crashed, true);
        const auto shutdownError = service.errorString();
        require(shutdownError.isEmpty(), shutdownError.toUtf8().constData());
    }

    {
        note::application::SessionRecoveryService service(directory);
        const auto deferred = service.start();
        require(!deferred.previousRunClean && service.recoveries().size() == 1,
                "Later preserves recovery data for another launch");
        service.finishNormally(deferred);
    }

    {
        note::application::SessionRecoveryService service(directory);
        const auto clean = service.start();
        require(clean.previousRunClean && clean.documents.size() == 3,
                "normal shutdown restores session without recovery prompt");
        service.finishNormally(clean);
    }

    {
        QFile corrupt(QDir(directory).filePath("broken.recovery"));
        require(corrupt.open(QIODevice::WriteOnly), "corruption fixture open failed");
        require(corrupt.write("partial") == 7, "corruption fixture write failed");
        corrupt.close();
        note::application::SessionRecoveryService service(directory);
        auto state = service.start();
        state.previousRunClean = false;
        const auto snapshots = service.recoveries();
        require(!snapshots.empty() && !snapshots.front().error.isEmpty(),
                "corrupt or interrupted snapshots are reported");
        service.removeRecovery("broken");
        service.finishNormally(state);
    }
    return 0;
}
