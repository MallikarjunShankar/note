#include "application/document_workflow.hpp"
#include "ui/command_palette.hpp"
#include "ui/editor_view.hpp"
#include "ui/settings_dialog.hpp"
#include "ui/main_window.hpp"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMenu>
#include <QLineEdit>
#include <QLabel>
#include <QKeySequenceEdit>
#include <QListWidget>
#include <QProgressDialog>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTabBar>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void sendKey(note::ui::EditorView& view, int key, Qt::KeyboardModifiers modifiers = {},
             const QString& text = {}) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(view.renderer(), &event);
}

void sendMouseClick(QWidget& widget, const QPointF& position,
                    Qt::KeyboardModifiers modifiers = {}) {
    QMouseEvent press(QEvent::MouseButtonPress, position, position, position,
                      Qt::LeftButton, Qt::LeftButton, modifiers);
    QApplication::sendEvent(&widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, position, position, position,
                        Qt::LeftButton, Qt::NoButton, modifiers);
    QApplication::sendEvent(&widget, &release);
}

note::ui::EditorView* activeEditorView(note::ui::MainWindow& window) {
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("editorStack"));
    return stack ? qobject_cast<note::ui::EditorView*>(stack->currentWidget()) : nullptr;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    try {
        {
            note::commands::CommandRegistry registry;
            note::commands::KeybindingManager keys(QDir::temp().filePath(
                QStringLiteral("note_palette_smoke_keys.ini")));
            keys.setDefault("a.one", QKeySequence(Qt::CTRL | Qt::Key_1));
            int ran = 0;
            registry.registerCommand({"a.one", "One", "A", "first"}, [&] { ran = 1; });
            registry.registerCommand({"a.two", "Two", "A", "second"}, [&] { ran = 2; });
            registry.registerCommand({"a.off", "Off", "A", "disabled"}, [] {},
                                     [] { return false; });
            note::ui::CommandPalette palette(registry, nullptr, &keys);
            palette.show();
            auto* list = palette.findChild<QListWidget*>();
            auto* query = palette.findChild<QLineEdit*>();
            require(list && query && list->count() == 2, "palette hides disabled commands");
            require(list->item(0)->text().contains(
                        QKeySequence(Qt::CTRL | Qt::Key_1).toString(QKeySequence::NativeText)) &&
                    list->item(0)->toolTip() == QStringLiteral("first"),
                    "palette shows shortcut and description");
            QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
            QApplication::sendEvent(query, &up);
            require(list->currentRow() == 1, "Up in the query wraps the palette selection");
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(query, &enter);
            require(ran == 2, "Enter runs the selected palette command");
        }
        {
            // Keybinding conflicts are explained with readable command names.
            QTemporaryDir storage;
            require(storage.isValid(), "keybinding fixture directory");
            note::customisation::SettingsManager settings(storage.filePath(QStringLiteral("s.ini")));
            note::customisation::ThemeManager themes(storage.filePath(QStringLiteral("themes")));
            note::commands::KeybindingManager keys(storage.filePath(QStringLiteral("k.ini")));
            note::commands::CommandRegistry registry;
            keys.setDefault("file.save", QKeySequence(Qt::CTRL | Qt::Key_S));
            keys.setDefault("file.open", QKeySequence(Qt::CTRL | Qt::Key_O));
            registry.registerCommand({"file.save", "Save", "File", "Save"}, [] {});
            registry.registerCommand({"file.open", "Open…", "File", "Open"}, [] {});
            note::ui::SettingsDialog dialog(settings, themes, keys, {}, nullptr, &registry);
            auto* edit = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("file.open"));
            require(edit, "keybinding editor exists");
            edit->setKeySequence(QKeySequence(Qt::CTRL | Qt::Key_S));
            QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
            bool explained = false;
            for (auto* label : dialog.findChildren<QLabel*>())
                explained = explained || label->text() == QStringLiteral("Conflict with File: Save");
            require(explained && keys.binding("file.open") == QKeySequence(Qt::CTRL | Qt::Key_O),
                    "conflicting shortcut is rejected and names the other command");
        }
        {
            // Customisation controls the model supported but Settings did not expose.
            QTemporaryDir storage;
            require(storage.isValid(), "customisation fixture directory");
            note::customisation::SettingsManager settings(storage.filePath(QStringLiteral("s.ini")));
            note::customisation::ThemeManager themes(storage.filePath(QStringLiteral("themes")));
            note::commands::KeybindingManager keys(storage.filePath(QStringLiteral("k.ini")));
            note::commands::CommandRegistry registry;
            registry.registerCommand({"file.new", "New", "File", "New"}, [] {});
            registry.registerCommand({"file.save", "Save", "File", "Save"}, [] {});
            registry.registerCommand({"search.find", "Find", "Search", "Find"}, [] {});
            note::ui::SettingsDialog dialog(settings, themes, keys, {}, nullptr, &registry);
            auto* round = dialog.findChild<QCheckBox*>(QStringLiteral("pair_round"));
            auto* single = dialog.findChild<QCheckBox*>(QStringLiteral("pair_singleQuote"));
            auto* weight = dialog.findChild<QSpinBox*>(QStringLiteral("fontWeight"));
            auto* toolbarList = dialog.findChild<QListWidget*>(QStringLiteral("toolbarCommands"));
            auto* cursor = dialog.findChild<QLineEdit*>(QStringLiteral("themeColour_cursor"));
            auto* border = dialog.findChild<QLineEdit*>(QStringLiteral("themeColour_border"));
            auto* name = dialog.findChild<QLineEdit*>();
            QLineEdit* themeName = nullptr;
            for (auto* line : dialog.findChildren<QLineEdit*>())
                if (line->text() == settings.settings().themeName && !line->objectName().startsWith("theme"))
                    themeName = line;
            auto* reset = dialog.findChild<QPushButton*>(QStringLiteral("resetThemeValues"));
            int colourRows = 0;
            for (auto* line : dialog.findChildren<QLineEdit*>())
                colourRows += line->objectName().startsWith(QStringLiteral("themeColour_"));
            require(round && single && weight && toolbarList && toolbarList->count() == 3 &&
                        cursor && border && name && themeName && reset &&
                        colourRows == static_cast<int>(note::customisation::themeColourFields().size()),
                    "settings expose pairing, weight, toolbar, every theme colour and reset");
            const auto savedCursor = cursor->text();
            cursor->setText(QStringLiteral("#abcdef"));
            reset->click();
            require(cursor->text() == savedCursor, "Reset Theme Values discards unsaved colour edits");

            round->setChecked(false);
            weight->setValue(700);
            for (int index = 0; index < toolbarList->count(); ++index)
                toolbarList->item(index)->setCheckState(
                    index == 1 ? Qt::Checked : Qt::Unchecked);
            themeName->setText(QStringLiteral("Mine"));
            border->setText(QStringLiteral("#123456"));
            for (auto* button : dialog.findChildren<QAbstractButton*>())
                if (button->text() == QStringLiteral("&Apply") || button->text() == QStringLiteral("Apply"))
                    button->click();
            const auto& applied = settings.settings();
            require(!applied.pairing.at("round") && applied.pairing.at("singleQuote") &&
                        applied.fontWeight == 700 &&
                        applied.toolbarActions == QStringList{QStringLiteral("file.save")},
                    "pairing, font weight and toolbar contents apply from Settings");
            require(applied.themeName == QStringLiteral("Mine") &&
                        themes.loadTheme(QStringLiteral("Mine")).border == QColor("#123456"),
                    "a newly editable theme colour is saved with the custom theme");
        }
        {
            // Find Next after editing with the Find dialog closed must use fresh offsets.
            note::application::DocumentWorkflow searchWorkflow;
            note::ui::MainWindow searchWindow(searchWorkflow);
            searchWindow.show();
            searchWindow.findChild<QAction*>(QStringLiteral("newDocumentAction"))->trigger();
            auto* view = activeEditorView(searchWindow);
            require(view, "search fixture editor");
            for (const char* chunk : {"a", "b", " ", "a", "b"})
                sendKey(*view, Qt::Key_unknown, Qt::NoModifier, QString::fromLatin1(chunk));
            searchWindow.findChild<QAction*>(QStringLiteral("search_findAction"))->trigger();
            auto* panel = searchWindow.findChild<QDialog*>(QStringLiteral("searchPanel"));
            auto* pattern = panel ? panel->findChild<QLineEdit*>() : nullptr;
            require(panel && pattern, "search panel exposes its pattern field");
            pattern->setText(QStringLiteral("ab"));
            panel->hide();
            view->restoreCursorPosition(0);
            sendKey(*view, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("xyz"));
            searchWindow.findChild<QAction*>(QStringLiteral("search_nextAction"))->trigger();
            const auto selection = view->selections().selection(0);
            const auto begin = std::min(selection.anchor, selection.active);
            require(searchWorkflow.activeDocument()->buffer().textRange(begin, 2) == "ab" &&
                        std::max(selection.anchor, selection.active) - begin == 2,
                    "Find Next re-runs a stale search after edits with the panel closed");
            searchWorkflow.shutdown();
        }
        {
            // Large documents are searched in slices: typing a pattern returns
            // quickly, and Find Next waits for a match beyond the first slice.
            note::application::DocumentWorkflow searchWorkflow;
            note::ui::MainWindow searchWindow(searchWorkflow);
            searchWindow.show();
            searchWindow.findChild<QAction*>(QStringLiteral("newDocumentAction"))->trigger();
            auto* view = activeEditorView(searchWindow);
            require(view, "large search fixture editor");
            std::string text;
            while (text.size() < 12u * 1024 * 1024)
                text += "    const auto value = compute(index, buffer.size());\n";
            const auto expected = text.size();
            text += "needle";
            view->replaceRange(0, 0, text);
            searchWindow.findChild<QAction*>(QStringLiteral("search_findAction"))->trigger();
            auto* panel = searchWindow.findChild<QDialog*>(QStringLiteral("searchPanel"));
            auto* pattern = panel ? panel->findChild<QLineEdit*>() : nullptr;
            require(panel && pattern, "large search panel");
            pattern->setText(QStringLiteral("needle"));
            searchWindow.findChild<QAction*>(QStringLiteral("search_nextAction"))->trigger();
            QLabel* status = nullptr;
            for (auto* label : panel->findChildren<QLabel*>()) status = label;
            QElapsedTimer waited;
            waited.start();
            while (status->text().startsWith(QStringLiteral("Searching")) && waited.elapsed() < 60000)
                QApplication::processEvents();
            const auto selection = view->selections().selection(0);
            require(status->text() == QStringLiteral("1 match(es)") &&
                        std::min(selection.anchor, selection.active) == expected &&
                        std::max(selection.anchor, selection.active) == expected + 6,
                    "sliced search finds and selects a match at the end of 12 MiB");
            // Replace All right after typing must also replace matches the
            // background slices have not reached yet.
            view->replaceRange(0, 0, "needle ");
            searchWindow.findChild<QAction*>(QStringLiteral("search_replaceAction"))->trigger();
            const auto edits = panel->findChildren<QLineEdit*>();
            require(edits.size() >= 2, "replace field exists");
            edits[1]->setText(QStringLiteral("pin"));
            pattern->setText(QStringLiteral("needle"));
            QAbstractButton* replaceAll = nullptr;
            for (auto* button : panel->findChildren<QAbstractButton*>())
                if (button->text() == QStringLiteral("Replace All")) replaceAll = button;
            require(replaceAll, "Replace All button");
            replaceAll->click();
            const auto& replaced = searchWorkflow.activeDocument()->buffer();
            require(replaced.textRange(0, 4) == "pin " &&
                        replaced.textRange(replaced.sizeBytes() - 3, 3) == "pin",
                    "Replace All replaces matches beyond the first search slice");
            searchWorkflow.shutdown();
        }
        note::application::DocumentWorkflow workflow;
        note::ui::MainWindow window(workflow);
        window.show();
        auto* newAction = window.findChild<QAction*>(QStringLiteral("newDocumentAction"));
        auto* saveAction = window.findChild<QAction*>(QStringLiteral("saveDocumentAction"));
        auto* closeAction = window.findChild<QAction*>(QStringLiteral("closeDocumentAction"));
        auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabs"));
        auto* editorStack = window.findChild<QStackedWidget*>(QStringLiteral("editorStack"));
        require(newAction && saveAction && closeAction && tabs && editorStack,
                "main window exposes file actions, tabs, and editor surface");
        require(tabs->count() == 0 && !closeAction->isEnabled(), "empty workspace state");
        const auto action = [&](const char* name) {
            auto* found = window.findChild<QAction*>(QString::fromLatin1(name));
            require(found, name);
            return found;
        };
        for (const char* name : {"edit_undoAction", "edit_redoAction", "edit_indentAction",
                                 "edit_outdentAction", "search_findAction", "search_nextAction",
                                 "search_previousAction", "search_replaceAction",
                                 "view_nextTabAction", "view_previousTabAction"})
            require(!action(name)->isEnabled(), "document commands are disabled with no document");
        require(action("search_nextAction")->shortcut() == QKeySequence(QKeySequence::FindNext) &&
                action("view_nextTabAction")->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_Tab),
                "find-next and tab-switch commands have default shortcuts");
        for (const char* menuName : {"edit_indentAction", "search_nextAction", "view_nextTabAction"})
            require(!action(menuName)->associatedObjects().isEmpty(),
                    "new commands are reachable from a menu");

        newAction->trigger();
        newAction->trigger();
        require(tabs->count() == 2 && workflow.tabs().size() == 2,
                "New action creates two distinct tabs");
        require(workflow.activeDocument() &&
                workflow.activeDocument()->id() == *workflow.tabs().at(1),
                "new tab becomes active");

        require(action("view_nextTabAction")->isEnabled(), "tab switching enabled with two tabs");
        action("view_nextTabAction")->trigger();
        require(tabs->currentIndex() == 0, "next tab wraps to the first tab");
        action("view_previousTabAction")->trigger();
        require(tabs->currentIndex() == 1, "previous tab wraps to the last tab");
        require(!action("edit_undoAction")->isEnabled() && action("search_findAction")->isEnabled(),
                "undo is disabled for a fresh document; find is enabled");

        tabs->setCurrentIndex(0);
        require(workflow.activeDocument() &&
                workflow.activeDocument()->id() == *workflow.tabs().at(0),
                "tab selection activates its associated document");
        auto* active = workflow.activeDocument();
        auto* editor = activeEditorView(window);
        require(editor && &editor->buffer() == &active->buffer(),
                "active editor view references active document buffer");
        sendKey(*editor, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("A"));
        sendKey(*editor, Qt::Key_unknown, Qt::NoModifier, QString::fromUtf8("λ"));
        require(active->buffer().text() == "Aλ" && editor->cursors().primaryOffset() == 3,
                "keyboard inserts ASCII and Unicode through editing engine");
        require(action("edit_undoAction")->isEnabled() && !action("edit_redoAction")->isEnabled(),
                "undo becomes enabled after an edit");
        action("edit_indentAction")->trigger();
        require(active->buffer().text().starts_with(' '), "Indent Lines command indents");
        action("edit_outdentAction")->trigger();
        require(active->buffer().text() == "Aλ", "Outdent Lines command outdents");
        editor->setIndentationConfiguration(false, 2);
        editor->selectRange(0, active->buffer().sizeBytes());
        sendKey(*editor, Qt::Key_Tab);
        require(active->buffer().text() == "  Aλ",
                "Tab indents selected lines using the configured space width");
        sendKey(*editor, Qt::Key_Backtab, Qt::ShiftModifier);
        require(active->buffer().text() == "Aλ",
                "Shift+Tab outdents selected lines by one configured level");
        editor->selections().clearSelections();
        editor->cursors().setPrimaryCursor(active->buffer().sizeBytes());
        editor->refreshFromModel();
        editor->setIndentationConfiguration(true, 8);
        editor->cursors().setPrimaryCursor(0);
        sendKey(*editor, Qt::Key_Tab);
        require(active->buffer().text() == "\tAλ",
                "Tab inserts a tab when tab indentation is configured");
        sendKey(*editor, Qt::Key_Backtab, Qt::ShiftModifier);
        require(active->buffer().text() == "Aλ",
                "Shift+Tab removes a leading configured tab");
        editor->setIndentationConfiguration(false, 4);
        editor->cursors().setPrimaryCursor(active->buffer().sizeBytes());
        editor->refreshFromModel();
        bool editorContextMenuShown = false;
        QTimer::singleShot(0, [&] {
            auto* contextMenu = qobject_cast<QMenu*>(
                QApplication::activePopupWidget());
            editorContextMenuShown = contextMenu &&
                contextMenu->actions().contains(
                    window.findChild<QAction*>(
                        QStringLiteral("edit_undoAction"))) &&
                contextMenu->actions().contains(
                    window.findChild<QAction*>(
                        QStringLiteral("search_findAction")));
            if (contextMenu) contextMenu->close();
        });
        const auto globalContextPosition =
            editor->renderer()->viewport()->mapToGlobal(QPoint(12, 12));
        QContextMenuEvent contextEvent(QContextMenuEvent::Mouse, QPoint(12, 12),
                                       globalContextPosition);
        QApplication::sendEvent(editor->renderer()->viewport(), &contextEvent);
        require(editorContextMenuShown,
                "editor context menu exposes shared edit and search actions");
        sendKey(*editor, Qt::Key_Left);
        sendKey(*editor, Qt::Key_Backspace);
        require(active->buffer().text() == "λ", "cursor movement and backspace");
        sendKey(*editor, Qt::Key_Z, Qt::ControlModifier);
        require(active->buffer().text() == "Aλ", "undo restores typed text");
        sendKey(*editor, Qt::Key_Y, Qt::ControlModifier);
        require(active->buffer().text() == "λ", "redo reapplies typed text");
        editor->cursors().setPrimaryCursor(0);
        editor->selections().setSelection(0, 0, 0);
        const auto lambdaWidth =
            QFontMetricsF(editor->renderer()->configuration().font)
                .horizontalAdvance(QString::fromUtf8("λ"));
        const QPointF lambdaEnd(editor->renderer()->textOriginX() + lambdaWidth,
                                10);
        sendMouseClick(*editor->renderer()->viewport(), lambdaEnd, Qt::AltModifier);
        require(editor->cursors().cursorCount() == 2 &&
                    active->buffer().isScalarBoundary(
                        editor->cursors().secondaryCursorOffset(0)),
                "Alt+Click keeps Unicode cursor positions on scalar boundaries");
        sendMouseClick(*editor->renderer()->viewport(), lambdaEnd, Qt::AltModifier);
        require(editor->cursors().cursorCount() == 1,
                "Alt+Click toggles the Unicode secondary cursor off");
        editor->cursors().setPrimaryCursor(active->buffer().sizeBytes());
        editor->selections().setSelection(0, active->buffer().sizeBytes(),
                                          active->buffer().sizeBytes());
        sendKey(*editor, Qt::Key_Left, Qt::ShiftModifier);
        require(editor->selections().hasSelection(0),
                "Shift+Arrow extends a keyboard selection");
        sendKey(*editor, Qt::Key_A, Qt::ControlModifier);
        sendKey(*editor, Qt::Key_C, Qt::ControlModifier);
        require(QApplication::clipboard()->text() == QString::fromUtf8("λ"),
                "copy selected Unicode text");
        sendKey(*editor, Qt::Key_X, Qt::ControlModifier);
        require(active->buffer().text().empty(), "cut removes selected text");
        sendKey(*editor, Qt::Key_V, Qt::ControlModifier);
        require(active->buffer().text() == "λ", "paste inserts clipboard text");

        auto* viewport = editor->renderer()->viewport();
        const auto clickPosition = QPointF(
            editor->renderer()->textOriginX(),
            editor->renderer()->configuration().lineHeight > 0
                ? editor->renderer()->configuration().lineHeight / 2
                : 10);
        QMouseEvent press(QEvent::MouseButtonPress, clickPosition, clickPosition, clickPosition,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &press);
        require(editor->cursors().primaryOffset() == 0, "mouse click positions the cursor");
        editor->cursors().setPrimaryCursor(active->buffer().sizeBytes());
        QMouseEvent dragPress(QEvent::MouseButtonPress, clickPosition, clickPosition, clickPosition,
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &dragPress);
        const QPointF dragPosition(editor->renderer()->textOriginX() + 100,
                                   clickPosition.y());
        QMouseEvent dragMove(QEvent::MouseMove, dragPosition, dragPosition, dragPosition,
                             Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &dragMove);
        QMouseEvent dragRelease(QEvent::MouseButtonRelease, dragPosition, dragPosition,
                                dragPosition, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &dragRelease);
        require(editor->selections().hasSelection(0), "mouse drag selects text");
        sendKey(*editor, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
        require(active->buffer().text() == "x", "typing replaces mouse selection");
        sendKey(*editor, Qt::Key_Return);
        require(active->buffer().text() == "x\n", "Enter inserts a newline");

        tabs->setCurrentIndex(1);
        auto* secondEditor = activeEditorView(window);
        require(secondEditor && secondEditor != editor,
                "switching tabs selects its independent editor view");
        sendKey(*secondEditor, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("other"));
        require(workflow.activeDocument()->buffer().text() == "other",
                "second document edits independently");
        sendKey(*secondEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text().empty(),
                "second document undo restores its independent content");
        tabs->setCurrentIndex(0);
        editor = activeEditorView(window);
        require(editor && editor->cursors().primaryOffset() == 2,
                "tab switching preserves per-document cursor state");
        active = workflow.activeDocument();
        active->buffer().insert(0, "unsaved");
        window.refreshFromModel();
        require(tabs->tabText(0).endsWith(QLatin1Char('*')) && window.windowTitle().contains(QLatin1Char('*')),
                "modified marker is reflected in tab and window title");
        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Cancel)) prompt->button(QMessageBox::Cancel)->click();
        });
        require(QMetaObject::invokeMethod(tabs, "tabCloseRequested", Qt::DirectConnection,
                                          Q_ARG(int, 0)), "dirty tab close prompt dispatched");
        require(workflow.findDocument(active->id()) && tabs->count() == 2,
                "cancel close prompt keeps dirty tab open");

        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Discard)) prompt->button(QMessageBox::Discard)->click();
        });
        require(QMetaObject::invokeMethod(tabs, "tabCloseRequested", Qt::DirectConnection,
                                          Q_ARG(int, 0)), "discard tab close prompt dispatched");
        window.refreshFromModel();
        require(tabs->count() == 1 && workflow.activeDocument() &&
                workflow.activeDocument()->id() == *workflow.tabs().at(0),
                "discard closes dirty tab and selects its neighbor");

        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Discard)) prompt->button(QMessageBox::Discard)->click();
        });
        closeAction->trigger();
        require(tabs->count() == 0 && workflow.documentCount() == 0 &&
                !workflow.activeDocument() && !closeAction->isEnabled(),
                "closing final tab leaves a valid empty workspace");

        QTemporaryDir temporary;
        require(temporary.isValid(), "temporary directory unavailable");
        const auto largePath = temporary.filePath(QStringLiteral("cancel.txt"));
        QFile largeFile(largePath);
        require(largeFile.open(QIODevice::WriteOnly), "could not create cancellation fixture");
        require(largeFile.write(QByteArray(8 * 1024 * 1024, 'x')) == 8 * 1024 * 1024,
                "could not write cancellation fixture");
        largeFile.close();
        window.openPath(std::filesystem::path(largePath.toStdWString()));
        auto* progress = window.findChild<QProgressDialog*>(QStringLiteral("openProgressDialog"));
        require(progress && workflow.documentCount() == 0 && tabs->count() == 0,
                "open progress appears without publishing a partial tab");
        auto* cancelOpenButton = progress->findChild<QPushButton*>();
        require(cancelOpenButton != nullptr, "progress dialog exposes its cancel button");
        cancelOpenButton->click();
        QEventLoop cancellationLoop;
        QTimer cancellationPoll;
        cancellationPoll.setInterval(10);
        QObject::connect(&cancellationPoll, &QTimer::timeout, &cancellationLoop, [&] {
            if (!window.findChild<QProgressDialog*>(QStringLiteral("openProgressDialog"))) cancellationLoop.quit();
        });
        QTimer cancellationTimeout;
        cancellationTimeout.setSingleShot(true);
        QObject::connect(&cancellationTimeout, &QTimer::timeout, &cancellationLoop, &QEventLoop::quit);
        cancellationPoll.start();
        cancellationTimeout.start(5000);
        cancellationLoop.exec();
        cancellationPoll.stop();
        require(!window.findChild<QProgressDialog*>(QStringLiteral("openProgressDialog")) &&
                workflow.documentCount() == 0 && tabs->count() == 0,
                "cancel action closes progress without publishing a document or tab");

        const auto smallPath = temporary.filePath(QStringLiteral("success.txt"));
        QFile smallFile(smallPath);
        require(smallFile.open(QIODevice::WriteOnly) && smallFile.write("ready") == 5,
                "could not create successful-open fixture");
        smallFile.close();
        window.openPath(std::filesystem::path(smallPath.toStdWString()));
        QEventLoop openLoop;
        QTimer openPoll;
        openPoll.setInterval(10);
        QObject::connect(&openPoll, &QTimer::timeout, &openLoop, [&] {
            if (workflow.documentCount() == 1) openLoop.quit();
        });
        QTimer openTimeout;
        openTimeout.setSingleShot(true);
        QObject::connect(&openTimeout, &QTimer::timeout, &openLoop, &QEventLoop::quit);
        openPoll.start();
        openTimeout.start(5000);
        openLoop.exec();
        openPoll.stop();
        require(workflow.documentCount() == 1 && tabs->count() == 1 &&
                workflow.activeDocument()->buffer().text() == "ready",
                "successful async open publishes one complete document and tab");
        auto* openedEditor = activeEditorView(window);
        require(openedEditor != nullptr, "opened document gets an editor view");
        const auto font = openedEditor->renderer()->configuration().font;
        const std::pair<QString, QString> bracketPairs[] = {
            {QStringLiteral("("), QStringLiteral(")")},
            {QStringLiteral("["), QStringLiteral("]")},
            {QStringLiteral("{"), QStringLiteral("}")},
            {QStringLiteral("\""), QStringLiteral("\"")},
            {QStringLiteral("'"), QStringLiteral("'")}
        };
        for (const auto& pair : bracketPairs) {
            openedEditor->cursors().setPrimaryCursor(0);
            openedEditor->selections().clearSelections();
            openedEditor->selections().addSelection(0, 0);
            sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier, pair.first);
            const auto expected = (pair.first + pair.second + QStringLiteral("ready"))
                                      .toUtf8().toStdString();
            require(workflow.activeDocument()->buffer().text() == expected &&
                        openedEditor->cursors().primaryOffset() == 1,
                    "each configured bracket type auto-pairs and places the cursor inside");
            if (pair.first == QStringLiteral("(")) {
                const auto geometry =
                    openedEditor->renderer()->matchingBracketGeometry();
                require(geometry.size() == 2,
                        "matching brackets are highlighted around the cursor");
                sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier,
                        pair.second);
                require(openedEditor->cursors().primaryOffset() == 2 &&
                            workflow.activeDocument()->buffer().text() == expected,
                        "typing an existing closer skips it without inserting a duplicate");
                openedEditor->cursors().setPrimaryCursor(1);
                openedEditor->selections().setSelection(0, 1, 1);
                sendKey(*openedEditor, Qt::Key_Backspace);
                require(workflow.activeDocument()->buffer().text() == "ready",
                        "backspace deletes an empty pair together");
                sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
                require(workflow.activeDocument()->buffer().text() == expected &&
                            openedEditor->cursors().primaryOffset() == 1,
                        "undo restores an automatically paired edit and cursor");
                sendKey(*openedEditor, Qt::Key_Y, Qt::ControlModifier);
                require(workflow.activeDocument()->buffer().text() == "ready",
                        "redo reapplies empty-pair deletion");
            } else {
                sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
                require(workflow.activeDocument()->buffer().text() == "ready",
                        "paired quote and bracket insertions undo atomically");
            }
        }
        openedEditor->cursors().setPrimaryCursor(5);
        openedEditor->selections().clearSelections();
        openedEditor->selections().addSelection(5, 5);
        sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("'"));
        require(workflow.activeDocument()->buffer().text() == "ready'",
                "apostrophe following a word is not auto-paired");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "ready",
                "apostrophe insertion remains undoable");

        openedEditor->cursors().setPrimaryCursor(0);
        openedEditor->selections().clearSelections();
        openedEditor->selections().addSelection(0, 0);
        const auto secondCursorX =
            QFontMetricsF(font).horizontalAdvance(QStringLiteral("re"));
        const QPointF secondCursorPosition(
            openedEditor->renderer()->textOriginX() + secondCursorX, 10);
        sendMouseClick(*openedEditor->renderer()->viewport(),
                       secondCursorPosition, Qt::AltModifier);
        require(openedEditor->cursors().cursorCount() == 2 &&
                    openedEditor->selections().selectionCount() == 2 &&
                    openedEditor->renderer()->cursorGeometry(1).width() > 0,
                "Alt+Click creates and renders an independent secondary cursor");
        sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier,
                QStringLiteral("("));
        require(workflow.activeDocument()->buffer().text() == "()re()ady" &&
                    openedEditor->cursors().primaryOffset() == 1 &&
                    openedEditor->cursors().secondaryCursorOffset(0) == 5,
                "bracket pairing applies at all active cursors");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "ready" &&
                    openedEditor->cursors().primaryOffset() == 0 &&
                    openedEditor->cursors().secondaryCursorOffset(0) == 2,
                "multi-cursor pair insertion undoes as one transaction");
        openedEditor->cursors().setPrimaryCursor(1);
        openedEditor->cursors().setSecondaryCursor(0, 2);
        openedEditor->selections().setSelection(0, 0, 1);
        openedEditor->selections().setSelection(1, 2, 2);
        sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier,
                QStringLiteral("("));
        require(workflow.activeDocument()->buffer().text() == "(e()ady",
                "selected text replacement and unselected cursor pairing coexist");
        sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier,
                QStringLiteral(")"));
        require(workflow.activeDocument()->buffer().text() == "()e()ady",
                "a closer is skipped at an unselected cursor during mixed selection edits");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "(e()ady" &&
                    openedEditor->cursors().primaryOffset() == 1 &&
                    openedEditor->cursors().secondaryCursorOffset(0) == 3,
                "mixed-cursor closer skip undoes without changing the paired document");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "ready" &&
                    openedEditor->selections().hasSelection(0),
                "mixed selection replacement and pairing undo atomically");
        openedEditor->cursors().setPrimaryCursor(0);
        openedEditor->cursors().setSecondaryCursor(0, 2);
        openedEditor->selections().clearSelections();
        openedEditor->selections().addSelection(0, 0);
        openedEditor->selections().addSelection(2, 2);
        sendKey(*openedEditor, Qt::Key_Right, Qt::ShiftModifier);
        require(openedEditor->selections().hasSelection(0) &&
                    openedEditor->selections().hasSelection(1),
                "keyboard extension creates independent multi-cursor selections");
        sendKey(*openedEditor, Qt::Key_C, Qt::ControlModifier);
        require(QApplication::clipboard()->text() == QStringLiteral("r\na"),
                "copy combines selected text from each cursor");
        sendKey(*openedEditor, Qt::Key_X, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "edy",
                "cut removes all multi-cursor selections");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "ready" &&
                    openedEditor->selections().selectionCount() == 2,
                "undo restores multiple selections atomically");
        sendKey(*openedEditor, Qt::Key_Y, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "edy",
                "redo reapplies multi-selection cut");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        openedEditor->cursors().setPrimaryCursor(0);
        openedEditor->cursors().setSecondaryCursor(0, 2);
        openedEditor->selections().setSelection(0, 0, 0);
        openedEditor->selections().setSelection(1, 2, 2);
        sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("!"));
        require(workflow.activeDocument()->buffer().text() == "!re!ady",
                "insertion applies at every active cursor");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "ready" &&
                    openedEditor->cursors().primaryOffset() == 0 &&
                    openedEditor->cursors().secondaryCursorOffset(0) == 2,
                "undo restores multi-cursor positions after simultaneous insertion");
        sendKey(*openedEditor, Qt::Key_Right, Qt::ShiftModifier);
        sendKey(*openedEditor, Qt::Key_Right, Qt::ShiftModifier);
        sendKey(*openedEditor, Qt::Key_Right, Qt::ShiftModifier);
        require(openedEditor->cursors().cursorCount() == 1 &&
                    openedEditor->selections().selectionCount() == 1,
                "overlapping extended ranges deterministically retain the primary cursor");
        openedEditor->cursors().setPrimaryCursor(0);
        openedEditor->selections().setSelection(0, 0, 0);
        sendMouseClick(*openedEditor->renderer()->viewport(),
                       secondCursorPosition, Qt::AltModifier);
        require(openedEditor->cursors().cursorCount() == 2,
                "non-overlapping secondary cursor can be added after range normalization");
        openedEditor->cursors().setPrimaryCursor(1);
        openedEditor->cursors().setSecondaryCursor(0, 3);
        openedEditor->selections().setSelection(0, 1, 1);
        openedEditor->selections().setSelection(1, 3, 3);
        sendKey(*openedEditor, Qt::Key_Backspace);
        require(workflow.activeDocument()->buffer().text() == "edy",
                "backspace deletes at multiple cursors simultaneously");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "ready",
                "multi-cursor backspace undo restores document contents");
        openedEditor->cursors().setPrimaryCursor(0);
        openedEditor->cursors().setSecondaryCursor(0, 2);
        openedEditor->selections().clearSelections();
        openedEditor->selections().addSelection(0, 0);
        openedEditor->selections().addSelection(2, 2);
        sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier,
                QStringLiteral("("));
        openedEditor->cursors().setPrimaryCursor(1);
        openedEditor->cursors().setSecondaryCursor(0, 2);
        openedEditor->selections().clearSelections();
        openedEditor->selections().addSelection(1, 1);
        openedEditor->selections().addSelection(2, 2);
        sendKey(*openedEditor, Qt::Key_Backspace);
        require(workflow.activeDocument()->buffer().text() == "re()ady" &&
                    openedEditor->cursors().cursorCount() == 1,
                "overlapping paired and ordinary backspaces retain the primary edit");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "()re()ady",
                "paired deletion remains undoable after conflict resolution");
        sendKey(*openedEditor, Qt::Key_Z, Qt::ControlModifier);
        require(workflow.activeDocument()->buffer().text() == "ready",
                "conflict-resolved pair insertion remains in history");
        openedEditor->cursors().setPrimaryCursor(0);
        openedEditor->cursors().clearSecondaryCursors();
        openedEditor->cursors().addSecondaryCursor(2);
        openedEditor->selections().setSelection(0, 0, 0);
        openedEditor->selections().setSelection(1, 2, 2);
        sendMouseClick(*openedEditor->renderer()->viewport(),
                       secondCursorPosition, Qt::AltModifier);
        require(openedEditor->cursors().cursorCount() == 1 &&
                    openedEditor->selections().selectionCount() == 1,
                "Alt+Click removes the secondary cursor and matching selection");

        const auto clickX = QFontMetricsF(font).horizontalAdvance(QStringLiteral("r"));
        const QPointF doubleClickPosition(
            openedEditor->renderer()->textOriginX() + clickX, 10);
        QMouseEvent doubleClick(QEvent::MouseButtonDblClick, doubleClickPosition,
                                doubleClickPosition, doubleClickPosition, Qt::LeftButton,
                                Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(openedEditor->renderer()->viewport(), &doubleClick);
        require(openedEditor->selections().hasSelection(0),
                "double click selects a word");
        openedEditor->cursors().setPrimaryCursor(0);
        openedEditor->selections().clearSelections();
        openedEditor->selections().addSelection(0, 0);
        sendKey(*openedEditor, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("!"));
        require(workflow.activeDocument()->buffer().text() == "!ready" &&
                    workflow.activeDocument()->modified(),
                "opened document edits through the integrated editor");
        saveAction->trigger();
        QFile savedFile(smallPath);
        require(savedFile.open(QIODevice::ReadOnly) && savedFile.readAll() == QByteArray("!ready") &&
                    !workflow.activeDocument()->modified(),
                "Save action persists editor changes");
        openedEditor->cursors().setPrimaryCursor(4);
        QFile externalChange(smallPath);
        require(externalChange.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                    externalChange.write("reloaded") == 8,
                "could not prepare external reload fixture");
        externalChange.close();
        bool reloadSucceeded = false;
        QEventLoop reloadLoop;
        QTimer reloadTimeout;
        reloadTimeout.setSingleShot(true);
        QObject::connect(&reloadTimeout, &QTimer::timeout, &reloadLoop, &QEventLoop::quit);
        const auto reloadRequest = workflow.reloadAsync(
            workflow.activeDocument()->id(), true, {},
            [&](auto, const auto& result) {
                reloadSucceeded = static_cast<bool>(result);
                reloadLoop.quit();
            });
        require(reloadRequest != 0, "external reload request was not accepted");
        reloadTimeout.start(5000);
        reloadLoop.exec();
        window.refreshFromModel();
        auto* reloadedEditor = activeEditorView(window);
        require(reloadSucceeded && workflow.activeDocument()->buffer().text() == "reloaded" &&
                    reloadedEditor && reloadedEditor->cursors().primaryOffset() == 0,
                "reloaded content recreates editor state without stale offsets");
        closeAction->trigger();
        require(workflow.documentCount() == 0, "opened test document closes cleanly");

        newAction->trigger();
        workflow.activeDocument()->buffer().insert(0, "unsaved window document");
        window.refreshFromModel();
        std::string manyLines;
        manyLines.reserve(50000);
        for (int line = 0; line < 10000; ++line) manyLines += "line\n";
        workflow.activeDocument()->buffer().insert(0, manyLines);
        window.refreshFromModel();
        auto* largeEditor = activeEditorView(window);
        require(largeEditor &&
                    largeEditor->renderer()->visibleLineRange().end <
                        workflow.activeDocument()->buffer().lineCount(),
                "large document view renders only a viewport-sized line range");
        largeEditor->renderer()->scrollToLine(5000);
        require(largeEditor->renderer()->firstVisibleLine() == 5000,
                "large document remains vertically navigable");
        auto* settingsAction = window.findChild<QAction*>(
            QStringLiteral("settings_openAction"));
        require(settingsAction != nullptr, "settings command is available");
        const auto originalBackground =
            largeEditor->renderer()->configuration().background;
        bool previewApplied = false;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget());
            auto* background = dialog
                ? dialog->findChild<QLineEdit*>(
                      QStringLiteral("themeColour_background"))
                : nullptr;
            if (background) {
                background->setText(QStringLiteral("#010203"));
                previewApplied = largeEditor->renderer()->configuration().background ==
                                 QColor(QStringLiteral("#010203"));
            }
            if (dialog) dialog->reject();
        });
        settingsAction->trigger();
        require(previewApplied,
                "theme preview applies immediately");
        require(largeEditor->renderer()->configuration().background ==
                    originalBackground,
                "Cancel restores the saved theme");
        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Cancel)) prompt->button(QMessageBox::Cancel)->click();
        });
        require(!window.close() && workflow.documentCount() == 1,
                "cancelling window close retains all documents");
        QTimer::singleShot(0, [] {
            auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (prompt && prompt->button(QMessageBox::Discard)) prompt->button(QMessageBox::Discard)->click();
        });
        require(window.close() && workflow.documentCount() == 0,
                "discard confirmation closes the complete workspace");
        std::cout << "Main window offscreen smoke test passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Main window smoke failure: " << error.what() << '\n';
        return 1;
    }
}
