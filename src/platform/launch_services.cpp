#include "platform/launch_services.hpp"
#include "platform/path_resolver.hpp"

#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <array>
#include <utility>

namespace note::platform {
namespace {

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
QString shellQuote(const QString& value) {
    QString quoted = QStringLiteral("'");
    for (const auto character : value) {
        if (character == QLatin1Char('\'')) quoted += QStringLiteral("'\\''");
        else quoted += character;
    }
    quoted += QLatin1Char('\'');
    return quoted;
}
#endif

} // namespace

LaunchResult openInExternalApplication(const std::filesystem::path& file) {
    if (file.empty()) return {false, "No file path is available."};
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(pathToQString(file))))
        return {false, "The operating system could not open this file in its associated application."};
    return {true, {}};
}

LaunchResult openTerminalInDirectory(const std::filesystem::path& directory) {
    if (directory.empty()) return {false, "No directory is available for this document."};
    const auto qdirectory = QFileInfo(pathToQString(directory)).absoluteFilePath();
#if defined(Q_OS_WIN)
    if (QProcess::startDetached(QStringLiteral("wt.exe"), {QStringLiteral("-d"), qdirectory}, qdirectory))
        return {true, {}};
    if (QProcess::startDetached(QStringLiteral("cmd.exe"),
        {QStringLiteral("/c"), QStringLiteral("start"), QString{}, QStringLiteral("/D"), qdirectory,
         QStringLiteral("cmd.exe")}, qdirectory)) return {true, {}};
    return {false, "Could not start Windows Terminal or Command Prompt."};
#elif defined(Q_OS_MACOS)
    if (QProcess::startDetached(QStringLiteral("open"),
        {QStringLiteral("-a"), QStringLiteral("Terminal"), qdirectory}, qdirectory)) return {true, {}};
    return {false, "Could not start the macOS Terminal application."};
#else
    const auto command = QStringLiteral("cd -- %1 && exec \"${SHELL:-/bin/sh}\"").arg(shellQuote(qdirectory));
    const std::array<std::pair<const char*, QStringList>, 3> candidates{{
        {"x-terminal-emulator", {QStringLiteral("-e"), QStringLiteral("sh"), QStringLiteral("-lc"), command}},
        {"gnome-terminal", {QStringLiteral("--"), QStringLiteral("sh"), QStringLiteral("-lc"), command}},
        {"konsole", {QStringLiteral("-e"), QStringLiteral("sh"), QStringLiteral("-lc"), command}}
    }};
    for (const auto& [name, arguments] : candidates) {
        const auto executable = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (!executable.isEmpty() && QProcess::startDetached(executable, arguments, qdirectory)) return {true, {}};
    }
    return {false, "No supported system terminal application was found."};
#endif
}

} // namespace note::platform
