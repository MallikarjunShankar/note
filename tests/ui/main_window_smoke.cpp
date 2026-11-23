#include "application/document_workflow.hpp"
#include "ui/editor_view.hpp"
#include "ui/main_window.hpp"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMenu>
#include <QLineEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTabBar>
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

        newAction->trigger();
        newAction->trigger();
        require(tabs->count() == 2 && workflow.tabs().size() == 2,
                "New action creates two distinct tabs");
        require(workflow.activeDocument() &&
                workflow.activeDocument()->id() == *workflow.tabs().at(1),
                "new tab becomes active");

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
        editor->setIndentationConfiguration(false, 2);
        editor->selectRange(0, active->buffer().sizeBytes());
        sendKey(*editor, Qt::Key_Tab);
        require(active->buffer().text() == "  Aλ",
                "Tab indents selected lines using the configured space width");
        sendKey(*editor, Qt::Key_Tab, Qt::ShiftModifier);
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
        sendKey(*editor, Qt::Key_Tab, Qt::ShiftModifier);
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
                      QStringLiteral("themeBackgroundColour"))
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
