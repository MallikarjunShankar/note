#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include <unordered_map>

namespace note::customisation {

struct NoteSettings {
    QString themeName{QStringLiteral("Dark")};
    QString fontFamily;
    int fontSize{12};
    int fontWeight{400};
    int lineSpacing{120};
    int editorZoom{100};
    bool statusBarVisible{true};
    bool menuVisible{true};
    bool toolbarVisible{false};
    QStringList toolbarActions;
    QString indentationStyle{QStringLiteral("spaces")};
    int indentationWidth{4};
    bool automaticPairing{true};
    std::unordered_map<std::string, bool> pairing;
    bool whitespaceVisible{};
    bool indentationGuides{};
    bool currentLineHighlight{true};
    bool matchingOccurrences{};
    bool lineNumbers{true};
    bool relativeLineNumbers{};
    bool wordWrap{};
};

class SettingsManager final {
public:
    explicit SettingsManager(QString storageFile);
    [[nodiscard]] const NoteSettings& settings() const noexcept { return settings_; }
    void setSettings(NoteSettings settings);
    void resetToDefaults();
    bool load();
    bool save();

private:
    NoteSettings defaults() const;
    void validate(NoteSettings& settings) const;
    QString storageFile_;
    NoteSettings settings_;
};

} // namespace note::customisation
