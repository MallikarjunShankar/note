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
    for (const auto& field : themeColourFields()) {
        const QColor candidate(object.value(field.key).toString());
        if (candidate.isValid()) theme.*field.member = candidate;
    }
    // Themes saved before these fields existed take them from their own colours.
    if (!object.contains("tabBar")) theme.tabBar = theme.window;
    if (!object.contains("border")) theme.border = theme.indentationGuide;
    if (!object.contains("interfaceText")) theme.interfaceText = theme.foreground;
    const auto syntax = object.value("syntax").toObject();
    for (auto it = syntax.begin(); it != syntax.end(); ++it) {
        QColor candidate(it.value().toString());
        if (candidate.isValid()) theme.syntaxColours[it.key().toStdString()] = candidate;
    }
    return true;
}

QByteArray themeJson(const NoteTheme& theme) {
    QJsonObject object;
    object["name"] = theme.name;
    for (const auto& field : themeColourFields())
        object[field.key] = colourName(theme.*field.member);
    QJsonObject syntax;
    for (const auto& [category, colour] : theme.syntaxColours)
        syntax[QString::fromStdString(category)] = colourName(colour);
    object["syntax"] = syntax;
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}
} // namespace

std::span<const ThemeColourField> themeColourFields() {
    static constexpr ThemeColourField fields[] = {
        {"background", "Editor background", &NoteTheme::background},
        {"foreground", "Default text", &NoteTheme::foreground},
        {"cursor", "Cursor", &NoteTheme::cursor},
        {"selection", "Selection", &NoteTheme::selection},
        {"currentLine", "Current-line highlight", &NoteTheme::currentLine},
        {"lineNumber", "Line numbers", &NoteTheme::lineNumber},
        {"activeLineNumber", "Current line number", &NoteTheme::activeLineNumber},
        {"indentationGuide", "Indentation guides", &NoteTheme::indentationGuide},
        {"whitespace", "Whitespace indicators", &NoteTheme::whitespace},
        {"matchingOccurrence", "Matching occurrences", &NoteTheme::matchingOccurrence},
        {"matchingBracket", "Matching brackets", &NoteTheme::matchingBracket},
        {"searchMatch", "Search matches", &NoteTheme::searchMatch},
        {"window", "Window background", &NoteTheme::window},
        {"menu", "Menu background", &NoteTheme::menu},
        {"tabBar", "Tab bar background", &NoteTheme::tabBar},
        {"tab", "Inactive tab", &NoteTheme::tab},
        {"activeTab", "Active tab", &NoteTheme::activeTab},
        {"status", "Status bar", &NoteTheme::status},
        {"border", "Borders and separators", &NoteTheme::border},
        {"interfaceText", "Interface text and icons", &NoteTheme::interfaceText},
    };
    return fields;
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
    darkTheme_.tabBar = QColor("#181b20");
    darkTheme_.border = QColor("#323843");
    darkTheme_.interfaceText = QColor("#d7dee9");
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
    light.tabBar = QColor("#e7e9ed");
    light.border = QColor("#c9cdd3");
    light.interfaceText = QColor("#20242b");
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
    QSaveFile file(QDir(themeDirectory_).filePath(theme.name + ".json"));
    if (!file.open(QIODevice::WriteOnly)) return false;
    const auto payload = themeJson(theme);
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
    const auto payload = themeJson(theme);
    return file.write(payload) == payload.size() && file.commit();
}

} // namespace note::customisation
