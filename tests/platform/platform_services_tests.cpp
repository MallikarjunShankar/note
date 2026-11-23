#include "platform/launch_services.hpp"
#include "platform/path_resolver.hpp"

#include <QCoreApplication>
#include <QFile>
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

    const auto filePath = temporary.filePath(QStringLiteral("note-λ.txt"));
    QFile file(filePath);
    require(file.open(QIODevice::WriteOnly), "could not create path fixture");
    file.write("note");
    file.close();

    const auto path = note::platform::pathFromQString(filePath);
    require(note::platform::pathToQString(path) == filePath,
            "Unicode filesystem paths round-trip through the platform adapter");
    const auto missingFile = note::platform::openInExternalApplication(
        note::platform::pathFromQString(temporary.filePath("missing.txt")));
    require(!missingFile.success && !missingFile.error.empty(),
            "external opening reports missing files without launching");
    const auto missingDirectory = note::platform::openTerminalInDirectory(
        note::platform::pathFromQString(temporary.filePath("missing")));
    require(!missingDirectory.success && !missingDirectory.error.empty(),
            "terminal launching reports missing directories without launching");
    return 0;
}
