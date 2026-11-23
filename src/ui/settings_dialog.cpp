#include "ui/settings_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QKeySequenceEdit>
#include <QAbstractItemView>

namespace note::ui {

SettingsDialog::SettingsDialog(customisation::SettingsManager& settings,
                               customisation::ThemeManager& themes,
                               commands::KeybindingManager& keybindings,
                               std::function<void()> applyCallback,
                               QWidget* parent)
    : QDialog(parent), settings_(settings), themes_(themes),
      applyCallback_(std::move(applyCallback)), keybindings_(keybindings) {
    setWindowTitle(tr("Settings"));
    setObjectName(QStringLiteral("settingsDialog"));
    resize(620, 520);
    connect(&themes_, &customisation::ThemeManager::themeChanged,
            this, [this](const customisation::NoteTheme&) {
        if (applyCallback_) applyCallback_();
    });
    auto* layout = new QVBoxLayout(this);
    auto* search = new QLineEdit(this);
    search->setPlaceholderText(tr("Search settings"));
    layout->addWidget(search);
    auto* tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    auto* appearance = new QWidget(tabs);
    auto* appearanceLayout = new QFormLayout(appearance);
    auto* theme = new QComboBox(appearance);
    for (const auto& builtIn : themes_.builtInThemes())
        theme->addItem(builtIn.name);
    for (const auto& custom : themes_.customThemeNames())
        theme->addItem(custom);
    theme->setCurrentText(settings_.settings().themeName);
    appearanceLayout->addRow(tr("Theme"), theme);
    auto* font = new QFontComboBox(appearance);
    font->setCurrentFont(QFont(settings_.settings().fontFamily));
    appearanceLayout->addRow(tr("Font family"), font);
    auto* fontSize = new QSpinBox(appearance);
    fontSize->setRange(6, 72);
    fontSize->setValue(settings_.settings().fontSize);
    appearanceLayout->addRow(tr("Font size"), fontSize);
    auto* lineSpacing = new QSpinBox(appearance);
    lineSpacing->setRange(80, 200);
    lineSpacing->setSuffix("%");
    lineSpacing->setValue(settings_.settings().lineSpacing);
    appearanceLayout->addRow(tr("Line spacing"), lineSpacing);
    auto* zoom = new QSpinBox(appearance);
    zoom->setRange(50, 300);
    zoom->setSuffix("%");
    zoom->setValue(settings_.settings().editorZoom);
    appearanceLayout->addRow(tr("Editor zoom"), zoom);
    tabs->addTab(appearance, tr("Appearance"));

    auto* layoutPage = new QWidget(tabs);
    auto* layoutForm = new QFormLayout(layoutPage);
    auto* statusBar = new QCheckBox(tr("Show status bar"), layoutPage);
    statusBar->setChecked(settings_.settings().statusBarVisible);
    auto* menuVisible = new QCheckBox(tr("Show menu"), layoutPage);
    menuVisible->setChecked(settings_.settings().menuVisible);
    auto* toolbar = new QCheckBox(tr("Show toolbar"), layoutPage);
    toolbar->setChecked(settings_.settings().toolbarVisible);
    layoutForm->addRow(statusBar);
    layoutForm->addRow(menuVisible);
    layoutForm->addRow(toolbar);
    tabs->addTab(layoutPage, tr("Layout"));

    auto* editing = new QWidget(tabs);
    auto* editingForm = new QFormLayout(editing);
    auto* indentStyle = new QComboBox(editing);
    indentStyle->addItem(tr("Spaces"), "spaces");
    indentStyle->addItem(tr("Tabs"), "tabs");
    indentStyle->setCurrentIndex(settings_.settings().indentationStyle == "tabs" ? 1 : 0);
    auto* indentWidth = new QSpinBox(editing);
    indentWidth->setRange(1, 16);
    indentWidth->setValue(settings_.settings().indentationWidth);
    auto* pairing = new QCheckBox(tr("Automatic pairing"), editing);
    pairing->setChecked(settings_.settings().automaticPairing);
    auto* whitespace = new QCheckBox(tr("Whitespace"), editing);
    whitespace->setChecked(settings_.settings().whitespaceVisible);
    auto* guides = new QCheckBox(tr("Indentation guides"), editing);
    guides->setChecked(settings_.settings().indentationGuides);
    auto* currentLine = new QCheckBox(tr("Current line"), editing);
    currentLine->setChecked(settings_.settings().currentLineHighlight);
    auto* occurrences = new QCheckBox(tr("Matching occurrences"), editing);
    occurrences->setChecked(settings_.settings().matchingOccurrences);
    auto* lineNumbers = new QCheckBox(tr("Line numbers"), editing);
    lineNumbers->setChecked(settings_.settings().lineNumbers);
    auto* relative = new QCheckBox(tr("Relative line numbers"), editing);
    relative->setChecked(settings_.settings().relativeLineNumbers);
    auto* wrap = new QCheckBox(tr("Word wrap"), editing);
    wrap->setChecked(settings_.settings().wordWrap);
    editingForm->addRow(tr("Indentation"), indentStyle);
    editingForm->addRow(tr("Indent width"), indentWidth);
    editingForm->addRow(pairing);
    editingForm->addRow(whitespace);
    editingForm->addRow(guides);
    editingForm->addRow(currentLine);
    editingForm->addRow(occurrences);
    editingForm->addRow(lineNumbers);
    editingForm->addRow(relative);
    editingForm->addRow(wrap);
    tabs->addTab(editing, tr("Editing"));

    auto* themePage = new QWidget(tabs);
    auto* themeForm = new QFormLayout(themePage);
    auto* themeName = new QLineEdit(themePage);
    themeName->setText(settings_.settings().themeName);
    themeForm->addRow(tr("Custom theme name"), themeName);
    auto* windowColour = new QLineEdit(themePage);
    auto* foregroundColour = new QLineEdit(themePage);
    auto* backgroundColour = new QLineEdit(themePage);
    auto* selectionColour = new QLineEdit(themePage);
    const auto activeTheme = themes_.loadTheme(settings_.settings().themeName);
    windowColour->setText(activeTheme.window.name());
    foregroundColour->setText(activeTheme.foreground.name());
    backgroundColour->setText(activeTheme.background.name());
    selectionColour->setText(activeTheme.selection.name());
    themeForm->addRow(tr("Window"), windowColour);
    themeForm->addRow(tr("Foreground"), foregroundColour);
    themeForm->addRow(tr("Background"), backgroundColour);
    themeForm->addRow(tr("Selection"), selectionColour);
    const auto preview = [this, themeName, windowColour, foregroundColour,
                          backgroundColour, selectionColour] {
        auto theme = themes_.loadTheme(settings_.settings().themeName);
        theme.name = themeName->text().isEmpty() ? theme.name : themeName->text();
        if (QColor(windowColour->text()).isValid())
            theme.window = QColor(windowColour->text());
        if (QColor(foregroundColour->text()).isValid())
            theme.foreground = QColor(foregroundColour->text());
        if (QColor(backgroundColour->text()).isValid())
            theme.background = QColor(backgroundColour->text());
        if (QColor(selectionColour->text()).isValid())
            theme.selection = QColor(selectionColour->text());
        if (themes_.validateTheme(theme))
            themes_.saveTheme(theme);
    };
    connect(themeName, &QLineEdit::textChanged, this, preview);
    connect(windowColour, &QLineEdit::textChanged, this, preview);
    connect(foregroundColour, &QLineEdit::textChanged, this, preview);
    connect(backgroundColour, &QLineEdit::textChanged, this, preview);
    connect(selectionColour, &QLineEdit::textChanged, this, preview);
    auto* importTheme = new QPushButton(tr("Import Theme"), themePage);
    auto* exportTheme = new QPushButton(tr("Export Theme"), themePage);
    themeForm->addRow(importTheme);
    themeForm->addRow(exportTheme);
    tabs->addTab(themePage, tr("Theme Editor"));

    auto* keyPage = new QWidget(tabs);
    auto* keyLayout = new QVBoxLayout(keyPage);
    auto* keyTable = new QTableWidget(static_cast<int>(keybindings_.bindings().size()),
                                      2, keyPage);
    keyTable->setHorizontalHeaderLabels({tr("Command"), tr("Shortcut")});
    keyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    int row = 0;
    std::vector<std::pair<std::string, QKeySequenceEdit*>> editors;
    for (const auto& binding : keybindings_.bindings()) {
        keyTable->setItem(row, 0,
            new QTableWidgetItem(QString::fromStdString(binding.commandId)));
        auto* edit = new QKeySequenceEdit(binding.sequence, keyPage);
        edit->setObjectName(QString::fromStdString(binding.commandId));
        keyTable->setCellWidget(row, 1, edit);
        editors.emplace_back(binding.commandId, edit);
        ++row;
    }
    auto* keyFeedback = new QLabel(keyPage);
    keyLayout->addWidget(keyTable);
    keyLayout->addWidget(keyFeedback);
    auto* restoreKeys = new QPushButton(tr("Restore Default Shortcuts"), keyPage);
    keyLayout->addWidget(restoreKeys);
    connect(restoreKeys, &QPushButton::clicked, this,
            [this, editors, keyFeedback] {
        keybindings_.restoreDefaults();
        for (const auto& [id, edit] : editors)
            edit->setKeySequence(keybindings_.binding(id));
        keyFeedback->setText(tr("Default shortcuts restored"));
        if (applyCallback_) applyCallback_();
    });
    for (const auto& [id, edit] : editors) {
        connect(edit, &QKeySequenceEdit::editingFinished, this,
                [this, id, edit, keyFeedback] {
            std::string conflict;
            if (!keybindings_.setBinding(id, edit->keySequence(), &conflict)) {
                edit->setKeySequence(keybindings_.binding(id));
                keyFeedback->setText(conflict.empty()
                    ? tr("Shortcut could not be set")
                    : tr("Conflict with %1").arg(QString::fromStdString(conflict)));
            } else {
                keyFeedback->setText(tr("Shortcut updated"));
                if (applyCallback_) applyCallback_();
            }
        });
    }
    tabs->addTab(keyPage, tr("Keybindings"));

    connect(importTheme, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, tr("Import Theme"), {},
                                                        tr("Note Themes (*.json)"));
        if (!path.isEmpty()) themes_.importTheme(path);
    });
    connect(exportTheme, &QPushButton::clicked, this, [this, themeName,
            windowColour, foregroundColour, backgroundColour, selectionColour] {
        const auto path = QFileDialog::getSaveFileName(this, tr("Export Theme"), {},
                                                        tr("Note Themes (*.json)"));
        if (path.isEmpty()) return;
        auto exported = themes_.loadTheme(settings_.settings().themeName);
        exported.name = themeName->text();
        exported.window = QColor(windowColour->text());
        exported.foreground = QColor(foregroundColour->text());
        exported.background = QColor(backgroundColour->text());
        exported.selection = QColor(selectionColour->text());
        themes_.exportTheme(exported, path);
    });

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Apply | QDialogButtonBox::Ok |
        QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    const auto applySettings = [this, theme, font, fontSize, lineSpacing, zoom,
                                statusBar, menuVisible, toolbar, indentStyle,
                                indentWidth, pairing, whitespace, guides,
                                currentLine, occurrences, lineNumbers, relative,
                                wrap, themeName, windowColour, foregroundColour,
                                backgroundColour, selectionColour] {
        auto value = settings_.settings();
        value.themeName = theme->currentText();
        value.fontFamily = font->currentFont().family();
        value.fontSize = fontSize->value();
        value.lineSpacing = lineSpacing->value();
        value.editorZoom = zoom->value();
        value.statusBarVisible = statusBar->isChecked();
        value.menuVisible = menuVisible->isChecked();
        value.toolbarVisible = toolbar->isChecked();
        value.indentationStyle = indentStyle->currentData().toString();
        value.indentationWidth = indentWidth->value();
        value.automaticPairing = pairing->isChecked();
        value.whitespaceVisible = whitespace->isChecked();
        value.indentationGuides = guides->isChecked();
        value.currentLineHighlight = currentLine->isChecked();
        value.matchingOccurrences = occurrences->isChecked();
        value.lineNumbers = lineNumbers->isChecked();
        value.relativeLineNumbers = relative->isChecked();
        value.wordWrap = wrap->isChecked();
        settings_.setSettings(value);
        auto custom = themes_.loadTheme(theme->currentText());
        custom.name = themeName->text().isEmpty() ? custom.name : themeName->text();
        if (QColor(windowColour->text()).isValid())
            custom.window = QColor(windowColour->text());
        if (QColor(foregroundColour->text()).isValid())
            custom.foreground = QColor(foregroundColour->text());
        if (QColor(backgroundColour->text()).isValid())
            custom.background = QColor(backgroundColour->text());
        if (QColor(selectionColour->text()).isValid())
            custom.selection = QColor(selectionColour->text());
        if (custom.name != "Dark" && custom.name != "Light")
            themes_.saveTheme(custom);
        settings_.save();
        if (applyCallback_) applyCallback_();
    };

    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, applySettings);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, applySettings] {
        applySettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(search, &QLineEdit::textChanged, this, [tabs](const QString& query) {
        const auto needle = query.trimmed();
        for (int index = 0; index < tabs->count(); ++index) {
            tabs->setTabVisible(index, needle.isEmpty() ||
                tabs->tabText(index).contains(needle, Qt::CaseInsensitive));
        }
    });
}

} // namespace note::ui
