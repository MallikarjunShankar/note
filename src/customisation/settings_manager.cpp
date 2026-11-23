#include "customisation/settings_manager.hpp"

#include <QFontDatabase>
#include <QSettings>
#include <algorithm>

namespace note::customisation {

SettingsManager::SettingsManager(QString storageFile)
    : storageFile_(std::move(storageFile)), settings_(defaults()) {}

NoteSettings SettingsManager::defaults() const {
    NoteSettings value;
    value.fontFamily =
        QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    value.toolbarActions = QStringList{
        QStringLiteral("file.new"), QStringLiteral("file.open"),
        QStringLiteral("file.save"), QStringLiteral("search.find")};
    value.pairing = {
        {"round", true}, {"square", true}, {"curly", true}, {"doubleQuote", true},
        {"singleQuote", true}};
    return value;
}

void SettingsManager::validate(NoteSettings& value) const {
    value.fontSize = std::clamp(value.fontSize, 6, 72);
    value.fontWeight = std::clamp(value.fontWeight, 100, 900);
    value.lineSpacing = std::clamp(value.lineSpacing, 80, 200);
    value.editorZoom = std::clamp(value.editorZoom, 50, 300);
    value.indentationWidth = std::clamp(value.indentationWidth, 1, 16);
    if (value.indentationStyle != "tabs") value.indentationStyle = "spaces";
    bool safeThemeName = !value.themeName.isEmpty() &&
                         value.themeName.size() <= 64 &&
                         value.themeName.trimmed() == value.themeName &&
                         value.themeName != "." && value.themeName != "..";
    for (const auto character : value.themeName) {
        if (character.isNull() ||
            character.category() == QChar::Other_Control ||
            QStringLiteral("<>:\"/\\|?*").contains(character))
            safeThemeName = false;
    }
    if (!safeThemeName)
        value.themeName = "Dark";
    if (value.fontFamily.isEmpty())
        value.fontFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

void SettingsManager::setSettings(NoteSettings settings) {
    validate(settings);
    settings_ = std::move(settings);
}

void SettingsManager::resetToDefaults() {
    settings_ = defaults();
}

bool SettingsManager::load() {
    QSettings settings(storageFile_, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) return false;
    auto value = defaults();
    value.themeName = settings.value("theme/name", value.themeName).toString();
    value.fontFamily = settings.value("typography/fontFamily", value.fontFamily).toString();
    value.fontSize = settings.value("typography/fontSize", value.fontSize).toInt();
    value.fontWeight = settings.value("typography/fontWeight", value.fontWeight).toInt();
    value.lineSpacing = settings.value("typography/lineSpacing", value.lineSpacing).toInt();
    value.editorZoom = settings.value("typography/editorZoom", value.editorZoom).toInt();
    value.statusBarVisible = settings.value("layout/statusBarVisible", true).toBool();
    value.menuVisible = settings.value("layout/menuVisible", true).toBool();
    value.toolbarVisible = settings.value("layout/toolbarVisible", false).toBool();
    value.toolbarActions = settings.value("layout/toolbarActions", value.toolbarActions).toStringList();
    value.indentationStyle = settings.value("editing/indentationStyle", value.indentationStyle).toString();
    value.indentationWidth = settings.value("editing/indentationWidth", value.indentationWidth).toInt();
    value.automaticPairing = settings.value("editing/automaticPairing", true).toBool();
    value.whitespaceVisible = settings.value("editing/whitespaceVisible", false).toBool();
    value.indentationGuides = settings.value("editing/indentationGuides", false).toBool();
    value.currentLineHighlight = settings.value("editing/currentLineHighlight", true).toBool();
    value.matchingOccurrences = settings.value("editing/matchingOccurrences", false).toBool();
    value.lineNumbers = settings.value("editing/lineNumbers", true).toBool();
    value.relativeLineNumbers = settings.value("editing/relativeLineNumbers", false).toBool();
    value.wordWrap = settings.value("editing/wordWrap", false).toBool();
    settings.beginGroup("pairing");
    for (const auto& [name, fallback] : defaults().pairing) {
        value.pairing[name] =
            settings.value(QString::fromStdString(name), fallback).toBool();
    }
    settings.endGroup();
    validate(value);
    settings_ = std::move(value);
    return true;
}

bool SettingsManager::save() {
    validate(settings_);
    QSettings settings(storageFile_, QSettings::IniFormat);
    settings.setValue("theme/name", settings_.themeName);
    settings.setValue("typography/fontFamily", settings_.fontFamily);
    settings.setValue("typography/fontSize", settings_.fontSize);
    settings.setValue("typography/fontWeight", settings_.fontWeight);
    settings.setValue("typography/lineSpacing", settings_.lineSpacing);
    settings.setValue("typography/editorZoom", settings_.editorZoom);
    settings.setValue("layout/statusBarVisible", settings_.statusBarVisible);
    settings.setValue("layout/menuVisible", settings_.menuVisible);
    settings.setValue("layout/toolbarVisible", settings_.toolbarVisible);
    settings.setValue("layout/toolbarActions", settings_.toolbarActions);
    settings.setValue("editing/indentationStyle", settings_.indentationStyle);
    settings.setValue("editing/indentationWidth", settings_.indentationWidth);
    settings.setValue("editing/automaticPairing", settings_.automaticPairing);
    settings.setValue("editing/whitespaceVisible", settings_.whitespaceVisible);
    settings.setValue("editing/indentationGuides", settings_.indentationGuides);
    settings.setValue("editing/currentLineHighlight", settings_.currentLineHighlight);
    settings.setValue("editing/matchingOccurrences", settings_.matchingOccurrences);
    settings.setValue("editing/lineNumbers", settings_.lineNumbers);
    settings.setValue("editing/relativeLineNumbers", settings_.relativeLineNumbers);
    settings.setValue("editing/wordWrap", settings_.wordWrap);
    settings.beginGroup("pairing");
    for (const auto& [name, enabled] : settings_.pairing)
        settings.setValue(QString::fromStdString(name), enabled);
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

} // namespace note::customisation
