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

    // Regression: note.exe was built for the console subsystem, so starting it
    // from Explorer also opened a console window. PE subsystem 2 = Windows GUI.
    if (argc > 1) {
        QFile executable(QString::fromLocal8Bit(argv[1]));
        require(executable.open(QIODevice::ReadOnly), "note executable is readable");
        const auto image = executable.read(4096);
        require(image.size() > 0x40 && image.startsWith("MZ"), "note executable is a PE image");
        const auto peOffset = static_cast<unsigned char>(image[0x3C]) |
                              static_cast<unsigned char>(image[0x3D]) << 8;
        const auto subsystemOffset = peOffset + 4 + 20 + 68; // signature, COFF header
        require(image.size() > subsystemOffset + 1 &&
                    image.mid(peOffset, 4) == QByteArray("PE\0\0", 4),
                "note executable has a PE header");
        require(static_cast<unsigned char>(image[subsystemOffset]) == 2,
                "note.exe uses the Windows GUI subsystem");
    }
    return 0;
}
