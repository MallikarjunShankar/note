#include "customisation/theme_manager.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

namespace note::customisation {
namespace {
QString colourName(const QColor& colour) { return colour.name(QColor::HexRgb); }
bool validThemeName(const QString& name) {
    if (name.isEmpty() || name.size() > 64 || name.trimmed() != name ||
        name == "." || name == "..")
        return false;
    for (const auto character : name) {
        if (character.isNull() || character.category() == QChar::Other_Control ||
            QStringLiteral("<>:\"/\\|?*").contains(character))
            return false;
    }
    return true;
}

bool readTheme(const QJsonObject& object, const NoteTheme& fallback, NoteTheme& theme) {
    const auto name = object.value("name").toString();
    if (name.isEmpty()) return false;
    theme = fallback;
    theme.name = name;
    auto readColour = [&](const char* key, QColor& target) {
        const auto text = object.value(key).toString();
        QColor candidate(text);
        if (candidate.isValid()) target = candidate;
    };
    readColour("window", theme.window);
    readColour("menu", theme.menu);
    readColour("tab", theme.tab);
    readColour("activeTab", theme.activeTab);
    readColour("status", theme.status);
    readColour("foreground", theme.foreground);
    readColour("background", theme.background);
    readColour("cursor", theme.cursor);
    readColour("selection", theme.selection);
    readColour("currentLine", theme.currentLine);
    readColour("lineNumber", theme.lineNumber);
    readColour("activeLineNumber", theme.activeLineNumber);
    readColour("indentationGuide", theme.indentationGuide);
    readColour("whitespace", theme.whitespace);
    readColour("matchingOccurrence", theme.matchingOccurrence);
    readColour("matchingBracket", theme.matchingBracket);
    readColour("searchMatch", theme.searchMatch);
    const auto syntax = object.value("syntax").toObject();
    for (auto it = syntax.begin(); it != syntax.end(); ++it) {
        QColor candidate(it.value().toString());
        if (candidate.isValid()) theme.syntaxColours[it.key().toStdString()] = candidate;
    }
    return true;
}
}

ThemeManager::ThemeManager(QString themeDirectory, QObject* parent)
    : QObject(parent), themeDirectory_(std::move(themeDirectory)) {
    initialiseThemes();
}

void ThemeManager::initialiseThemes() {
    darkTheme_.name = "Dark";
    darkTheme_.window = QColor("#181b20");
    darkTheme_.menu = QColor("#20242b");
    darkTheme_.tab = QColor("#20242b");
    darkTheme_.activeTab = QColor("#303746");
    darkTheme_.status = QColor("#20242b");
    darkTheme_.foreground = QColor("#d7dee9");
    darkTheme_.background = QColor("#1e222a");
    darkTheme_.cursor = QColor("#f2f5f8");
    darkTheme_.selection = QColor("#3d5a80");
    darkTheme_.currentLine = QColor("#252b35");
    darkTheme_.lineNumber = QColor("#737d8c");
    darkTheme_.activeLineNumber = QColor("#d7dee9");
    darkTheme_.indentationGuide = QColor("#39414d");
    darkTheme_.whitespace = QColor("#687382");
    darkTheme_.matchingOccurrence = QColor("#3a4655");
    darkTheme_.matchingBracket = QColor("#66788a");
    darkTheme_.searchMatch = QColor("#6b5523");
}

std::vector<NoteTheme> ThemeManager::builtInThemes() const {
    NoteTheme light = darkTheme_;
    light.name = "Light";
    light.window = QColor("#e7e9ed");
    light.menu = QColor("#f4f5f7");
    light.tab = QColor("#e1e3e7");
    light.activeTab = QColor("#ffffff");
    light.status = QColor("#f4f5f7");
    light.foreground = QColor("#20242b");
    light.background = QColor("#ffffff");
    light.cursor = QColor("#20242b");
    light.selection = QColor("#b8d4f0");
    light.currentLine = QColor("#f1f4f7");
    light.lineNumber = QColor("#87909b");
    light.activeLineNumber = QColor("#20242b");
    light.indentationGuide = QColor("#d3d7dc");
    light.whitespace = QColor("#9aa2aa");
    light.matchingOccurrence = QColor("#d8e2ea");
    light.matchingBracket = QColor("#9aa8b5");
    light.searchMatch = QColor("#ffd866");
    return {darkTheme_, light};
}

std::vector<QString> ThemeManager::customThemeNames() const {
    std::vector<QString> names;
    QDir directory(themeDirectory_);
    if (!directory.exists()) return names;
    for (const auto& file : directory.entryList({ "*.json" }, QDir::Files)) {
        QFile input(directory.filePath(file));
        if (!input.open(QIODevice::ReadOnly)) continue;
        const auto object = QJsonDocument::fromJson(input.readAll()).object();
        NoteTheme theme;
        if (readTheme(object, darkTheme_, theme)) names.push_back(theme.name);
    }
    return names;
}

NoteTheme ThemeManager::loadTheme(const QString& name) const {
    if (previewTheme_) return *previewTheme_;
    for (const auto& theme : builtInThemes())
        if (theme.name == name) return theme;
    if (!validThemeName(name)) return darkTheme_;
    QFile input(QDir(themeDirectory_).filePath(name + ".json"));
    if (!input.open(QIODevice::ReadOnly)) return darkTheme_;
    NoteTheme theme;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(input.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() ||
        !readTheme(document.object(), darkTheme_, theme))
        return darkTheme_;
    return theme;
}

bool ThemeManager::validateTheme(const NoteTheme& theme) const {
    return validThemeName(theme.name) && theme.background.isValid() &&
           theme.foreground.isValid();
}

bool ThemeManager::setPreviewTheme(NoteTheme theme) {
    if (!validateTheme(theme)) return false;
    previewTheme_ = std::move(theme);
    emit themeChanged(*previewTheme_);
    return true;
}

void ThemeManager::clearPreviewTheme() noexcept {
    previewTheme_.reset();
}

bool ThemeManager::saveTheme(const NoteTheme& theme) {
    if (!validateTheme(theme)) return false;
    QDir().mkpath(themeDirectory_);
    QJsonObject object;
    object["name"] = theme.name;
    object["window"] = colourName(theme.window);
    object["menu"] = theme.menu.name(QColor::HexRgb);
    object["tab"] = theme.tab.name(QColor::HexRgb);
    object["activeTab"] = theme.activeTab.name(QColor::HexRgb);
    object["status"] = theme.status.name(QColor::HexRgb);
    object["foreground"] = theme.foreground.name(QColor::HexRgb);
    object["background"] = theme.background.name(QColor::HexRgb);
    object["cursor"] = theme.cursor.name(QColor::HexRgb);
    object["selection"] = theme.selection.name(QColor::HexRgb);
    object["currentLine"] = theme.currentLine.name(QColor::HexRgb);
    object["lineNumber"] = theme.lineNumber.name(QColor::HexRgb);
    object["activeLineNumber"] = theme.activeLineNumber.name(QColor::HexRgb);
    object["indentationGuide"] = theme.indentationGuide.name(QColor::HexRgb);
    object["whitespace"] = theme.whitespace.name(QColor::HexRgb);
    object["matchingOccurrence"] = theme.matchingOccurrence.name(QColor::HexRgb);
    object["matchingBracket"] = theme.matchingBracket.name(QColor::HexRgb);
    object["searchMatch"] = theme.searchMatch.name(QColor::HexRgb);
    QJsonObject syntax;
    for (const auto& [category, colour] : theme.syntaxColours)
        syntax[QString::fromStdString(category)] = colour.name(QColor::HexRgb);
    object["syntax"] = syntax;
    QSaveFile file(QDir(themeDirectory_).filePath(theme.name + ".json"));
    if (!file.open(QIODevice::WriteOnly)) return false;
    const auto payload = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        file.cancelWriting();
        return false;
    }
    emit themeChanged(theme);
    return true;
}

bool ThemeManager::importTheme(const QString& sourcePath, QString* importedName) {
    QFile input(sourcePath);
    if (!input.open(QIODevice::ReadOnly)) return false;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(input.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() ||
        document.object().value("name").toString().isEmpty())
        return false;
    NoteTheme theme;
    if (!readTheme(document.object(), darkTheme_, theme) || !validateTheme(theme))
        return false;
    if (!saveTheme(theme)) return false;
    if (importedName) *importedName = theme.name;
    return true;
}

bool ThemeManager::exportTheme(const NoteTheme& theme, const QString& destinationPath) const {
    if (!validateTheme(theme)) return false;
    QSaveFile file(destinationPath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QJsonObject object;
    object["name"] = theme.name;
    object["foreground"] = theme.foreground.name(QColor::HexRgb);
    object["background"] = theme.background.name(QColor::HexRgb);
    object["window"] = theme.window.name(QColor::HexRgb);
    object["selection"] = theme.selection.name(QColor::HexRgb);
    object["cursor"] = theme.cursor.name(QColor::HexRgb);
    object["searchMatch"] = theme.searchMatch.name(QColor::HexRgb);
    QJsonObject syntax;
    for (const auto& [category, colour] : theme.syntaxColours)
        syntax[QString::fromStdString(category)] = colour.name(QColor::HexRgb);
    object["syntax"] = syntax;
    const auto payload = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.write(payload) == payload.size() && file.commit();
}

} // namespace note::customisation
