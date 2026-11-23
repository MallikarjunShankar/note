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
    {
        // Regression: export wrote only 6 of the colours, so a round trip lost
        // the rest. Every editable colour must survive export and import.
        note::customisation::ThemeManager other(temporary.filePath("other-themes"));
        auto every = dark;
        every.name = "Every Colour";
        int shade = 1;
        for (const auto& field : note::customisation::themeColourFields())
            every.*(field.member) = QColor(shade, shade * 2, shade * 3), ++shade;
        const auto everyPath = temporary.filePath("every.json");
        require(themes.exportTheme(every, everyPath) && other.importTheme(everyPath),
                "full theme exports and imports");
        const auto restored = other.loadTheme("Every Colour");
        for (const auto& field : note::customisation::themeColourFields())
            require(restored.*(field.member) == every.*(field.member),
                    "every theme colour survives export and import");
    }
    {
        // Theme files from before tabBar/border/interfaceText derive them from
        // their own colours instead of the dark fallback.
        const auto legacyPath = temporary.filePath("legacy.json");
        QFile legacy(legacyPath);
        require(legacy.open(QIODevice::WriteOnly), "legacy theme fixture opens");
        legacy.write(R"({"name":"Legacy","window":"#eeeeee","foreground":"#111111",)"
                     R"("background":"#ffffff","indentationGuide":"#cccccc"})");
        legacy.close();
        require(themes.importTheme(legacyPath), "legacy theme imports");
        const auto loadedLegacy = themes.loadTheme("Legacy");
        require(loadedLegacy.tabBar == QColor("#eeeeee") &&
                    loadedLegacy.border == QColor("#cccccc") &&
                    loadedLegacy.interfaceText == QColor("#111111"),
                "missing new theme fields fall back to the theme's own colours");
    }

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
