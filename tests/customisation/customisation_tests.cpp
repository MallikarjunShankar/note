#include "customisation/settings_manager.hpp"
#include "customisation/theme_manager.hpp"

#include <QCoreApplication>
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
    settings.setSettings(value);
    require(settings.settings().fontSize == 72,
            "font size is validated");
    require(settings.settings().indentationWidth == 1,
            "indent width is validated");
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
    require(themes.importTheme(exportPath), "theme import succeeds");
    return 0;
}
