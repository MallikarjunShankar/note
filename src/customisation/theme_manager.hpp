#pragma once

#include <QColor>
#include <QObject>
#include <QString>

#include <optional>
#include <unordered_map>
#include <vector>

namespace note::customisation {

struct NoteTheme {
    QString name;
    QColor window;
    QColor menu;
    QColor tab;
    QColor activeTab;
    QColor status;
    QColor foreground;
    QColor background;
    QColor cursor;
    QColor selection;
    QColor currentLine;
    QColor lineNumber;
    QColor activeLineNumber;
    QColor indentationGuide;
    QColor whitespace;
    QColor matchingOccurrence;
    QColor matchingBracket;
    QColor searchMatch;
    std::unordered_map<std::string, QColor> syntaxColours;
};

class ThemeManager final : public QObject {
    Q_OBJECT
public:
    explicit ThemeManager(QString themeDirectory, QObject* parent = nullptr);
    [[nodiscard]] std::vector<NoteTheme> builtInThemes() const;
    [[nodiscard]] std::vector<QString> customThemeNames() const;
    [[nodiscard]] const NoteTheme& fallbackTheme() const noexcept { return darkTheme_; }
    [[nodiscard]] NoteTheme loadTheme(const QString& name) const;
    [[nodiscard]] bool validateTheme(const NoteTheme& theme) const;
    bool setPreviewTheme(NoteTheme theme);
    void clearPreviewTheme() noexcept;
    bool saveTheme(const NoteTheme& theme);
    bool importTheme(const QString& sourcePath, QString* importedName = nullptr);
    bool exportTheme(const NoteTheme& theme, const QString& destinationPath) const;
signals:
    void themeChanged(const NoteTheme& theme);

private:
    NoteTheme darkTheme_;
    std::optional<NoteTheme> previewTheme_;
    QString themeDirectory_;
    void initialiseThemes();
};

} // namespace note::customisation
