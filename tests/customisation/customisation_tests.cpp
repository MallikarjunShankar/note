#include "customisation/settings_manager.hpp"
#include "customisation/theme_manager.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory unavailable");

    note::customisation::SettingsManager settings(
        temporary.filePath("settings.ini"));
    auto value = settings.settings();
    value.fontSize = 999;
    value.indentationWidth = 0;
    value.themeName = "../outside";
    settings.setSettings(value);
    require(settings.settings().fontSize == 72,
            "font size is validated");
    require(settings.settings().indentationWidth == 1,
            "indent width is validated");
    require(settings.settings().themeName == "Dark",
            "unsafe theme names fall back to the default");
    require(settings.save(), "settings save succeeds");
    require(settings.load(), "settings load succeeds");

    note::customisation::ThemeManager themes(
        temporary.filePath("themes"));
    const auto dark = themes.loadTheme("Dark");
    require(themes.validateTheme(dark), "built-in theme validates");
    auto custom = dark;
    custom.name = "Test Theme";
    custom.background = QColor("#112233");
    require(themes.saveTheme(custom), "custom theme saves");
    const auto loaded = themes.loadTheme("Test Theme");
    require(loaded.background == QColor("#112233"),
            "custom theme restores");
    const auto exportPath = temporary.filePath("export.json");
    require(themes.exportTheme(custom, exportPath), "theme export succeeds");
    QString importedName;
    require(themes.importTheme(exportPath, &importedName) &&
            importedName == custom.name, "theme import returns its saved name");

    auto preview = dark;
    preview.name = "Preview Only";
    preview.background = QColor("#445566");
    require(themes.setPreviewTheme(preview), "valid theme previews");
    require(themes.loadTheme("Dark").background == preview.background,
            "preview applies without changing the saved theme");
    themes.clearPreviewTheme();
    require(themes.loadTheme("Dark").background == dark.background,
            "clearing a preview restores the saved theme");
    auto invalid = dark;
    invalid.name = "../settings";
    require(!themes.validateTheme(invalid),
            "theme names cannot escape the theme directory");

    const auto invalidThemePath = temporary.filePath("invalid-theme.json");
    QFile invalidTheme(invalidThemePath);
    require(invalidTheme.open(QIODevice::WriteOnly), "invalid theme fixture opens");
    invalidTheme.write("{\"name\":\"../settings\"}");
    invalidTheme.close();
    require(!themes.importTheme(invalidThemePath),
            "theme import rejects names that could escape the theme directory");
    return 0;
}
