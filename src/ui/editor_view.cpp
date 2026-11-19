#include "ui/editor_view.hpp"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <QWheelEvent>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace note::ui {

EditorView::EditorView(editor::TextBuffer& buffer,
                       editor::LineEnding preferredLineEnding,
                       QWidget* parent)
    : QWidget(parent), buffer_(buffer), cursors_(buffer), selections_(buffer),
      editing_(buffer, cursors_, selections_),
      history_(buffer, cursors_, selections_, editing_),
      preferredLineEnding_(preferredLineEnding) {
    setObjectName(QStringLiteral("editorView"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    renderer_ = new EditorRenderer(buffer_, cursors_, selections_, this);
    renderer_->setObjectName(QStringLiteral("editorRenderer"));
    renderer_->viewport()->installEventFilter(this);
    renderer_->installEventFilter(this);
    layout->addWidget(renderer_);
    setFocusProxy(renderer_);
    setAttribute(Qt::WA_StyledBackground, false);
}

editor::TextBuffer& EditorView::buffer() const noexcept { return buffer_; }
editor::CursorManager& EditorView::cursors() noexcept { return cursors_; }
editor::SelectionManager& EditorView::selections() noexcept { return selections_; }
EditorRenderer* EditorView::renderer() const noexcept { return renderer_; }

void EditorView::setDocumentChangedCallback(std::function<void()> callback) {
    documentChangedCallback_ = std::move(callback);
}

void EditorView::refreshFromModel() {
    cursors_.syncToBuffer();
    selections_.syncToBuffer();
    renderer_->refreshFromModel();
}

bool EditorView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress)
        return handleKeyPress(static_cast<QKeyEvent*>(event));
    if (watched == renderer_->viewport()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            return handleMousePress(static_cast<QMouseEvent*>(event));
        case QEvent::MouseMove:
            return handleMouseMove(static_cast<QMouseEvent*>(event));
        case QEvent::MouseButtonRelease:
            return handleMouseRelease(static_cast<QMouseEvent*>(event));
        case QEvent::MouseButtonDblClick:
            return handleMouseDoubleClick(static_cast<QMouseEvent*>(event));
        case QEvent::Wheel: {
            auto* wheel = static_cast<QWheelEvent*>(event);
            if (wheel->modifiers().testFlag(Qt::ShiftModifier) &&
                wheel->angleDelta().y() != 0) {
                renderer_->setHorizontalOffset(std::max<qreal>(
                    0, renderer_->horizontalOffset() - wheel->angleDelta().y()));
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool EditorView::handleKeyPress(QKeyEvent* event) {
    if (event->matches(QKeySequence::Copy)) {
        copySelection(false);
        return true;
    }
    if (event->matches(QKeySequence::Cut)) {
        copySelection(true);
        return true;
    }
    if (event->matches(QKeySequence::Paste)) {
        pasteClipboard();
        return true;
    }
    if (event->matches(QKeySequence::Undo)) {
        if (history_.undo()) afterEdit();
        return true;
    }
    if (event->matches(QKeySequence::Redo)) {
        if (history_.redo()) afterEdit();
        return true;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        setPrimarySelection(0, buffer_.sizeBytes());
        renderer_->viewport()->update();
        return true;
    }

    const auto modifiers = event->modifiers();
    if (modifiers.testFlag(Qt::ControlModifier) ||
        modifiers.testFlag(Qt::AltModifier) ||
        modifiers.testFlag(Qt::MetaModifier))
        return false;

    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Home:
    case Qt::Key_End:
        moveCursor(event->key(), shift);
        return true;
    case Qt::Key_Backspace:
        if (hasAnySelection()) (void)history_.deleteSelections();
        else (void)history_.backspace();
        afterEdit();
        return true;
    case Qt::Key_Delete:
        if (hasAnySelection()) (void)history_.deleteSelections();
        else (void)history_.deleteForward();
        afterEdit();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        insertNewlineWithIndent();
        afterEdit();
        return true;
    case Qt::Key_Tab:
        (void)history_.insertText("    ");
        afterEdit();
        return true;
    default:
        break;
    }

    const auto text = event->text();
    if (!text.isEmpty() && text.at(0).isPrint()) {
        const auto utf8 = text.toUtf8();
        if (hasAnySelection())
            (void)history_.replaceSelections(
                std::string_view(utf8.constData(), static_cast<std::size_t>(utf8.size())));
        else
            (void)history_.insertText(
                std::string_view(utf8.constData(), static_cast<std::size_t>(utf8.size())));
        afterEdit();
        return true;
    }
    return false;
}

void EditorView::moveCursor(int key, bool extendSelection) {
    const auto previous = cursors_.primaryOffset();
    auto anchor = selections_.selectionCount() > 0 && selections_.hasSelection(0)
                      ? selections_.selection(0).anchor
                      : previous;
    if (!extendSelection && selections_.selectionCount() > 0 &&
        selections_.hasSelection(0) &&
        (key == Qt::Key_Left || key == Qt::Key_Right)) {
        const auto range = selections_.normalizedRange(0);
        cursors_.setPrimaryCursor(key == Qt::Key_Left ? range.begin : range.end);
    } else {
        switch (key) {
        case Qt::Key_Left: cursors_.moveLeft(); break;
        case Qt::Key_Right: cursors_.moveRight(); break;
        case Qt::Key_Up: cursors_.moveUp(); break;
        case Qt::Key_Down: cursors_.moveDown(); break;
        case Qt::Key_Home: {
            const auto location = cursors_.primaryLocation();
            cursors_.moveToLineColumn(location.line, 0);
            break;
        }
        case Qt::Key_End: {
            const auto location = cursors_.primaryLocation();
            const auto line = buffer_.lineRange(location.line);
            cursors_.moveToLineColumn(location.line, line.end - line.begin);
            break;
        }
        default: break;
        }
    }
    if (extendSelection) setPrimarySelection(anchor, cursors_.primaryOffset());
    else collapseSelectionsAt(cursors_.primaryOffset());
    renderer_->ensureCursorVisible(cursors_.primaryOffset());
    renderer_->viewport()->update();
}

void EditorView::setPrimarySelection(std::size_t anchor, std::size_t active) {
    if (selections_.selectionCount() == 0) selections_.addSelection(anchor, active);
    else selections_.setSelection(0, anchor, active);
}

void EditorView::collapseSelectionsAt(std::size_t offset) {
    selections_.clearSelections();
    selections_.addSelection(offset, offset);
}

bool EditorView::hasAnySelection() const {
    for (std::size_t index = 0; index < selections_.selectionCount(); ++index)
        if (selections_.hasSelection(index)) return true;
    return false;
}

void EditorView::afterEdit() {
    renderer_->refreshFromModel();
    renderer_->ensureCursorVisible(cursors_.primaryOffset());
    if (documentChangedCallback_) documentChangedCallback_();
}

void EditorView::insertNewlineWithIndent() {
    const auto cursor = cursors_.primaryOffset();
    const auto location = cursors_.primaryLocation();
    const auto lineRange = buffer_.lineRange(location.line);
    auto indentEnd = lineRange.begin;
    while (indentEnd < lineRange.end) {
        const auto byte = buffer_.byteAt(indentEnd);
        if (byte != ' ' && byte != '\t') break;
        ++indentEnd;
    }
    auto indentation = buffer_.textRange(lineRange.begin, indentEnd - lineRange.begin);
    if (cursor > lineRange.begin && buffer_.byteAt(cursor - 1) == '{')
        indentation.append(4, ' ');
    const auto indent = indentation.size();
    const auto afterCursor = cursor < buffer_.sizeBytes()
                                 ? buffer_.byteAt(cursor)
                                 : static_cast<unsigned char>(0);
    if (afterCursor == '}' && indent >= 4) indentation.resize(indent - 4);
    std::string insertion{"\n"};
    insertion.append(indentation);
    if (hasAnySelection())
        (void)history_.replaceSelections(insertion, preferredLineEnding_);
    else
        (void)history_.insertText(insertion, preferredLineEnding_);
}

void EditorView::copySelection(bool cut) {
    if (selections_.selectionCount() == 0 || !selections_.hasSelection(0)) return;
    const auto range = selections_.normalizedRange(0);
    const auto bytes = buffer_.textRange(range.begin, range.end - range.begin);
    QApplication::clipboard()->setText(
        QString::fromUtf8(bytes.data(), static_cast<qsizetype>(bytes.size())));
    if (cut) {
        (void)history_.deleteSelections();
        afterEdit();
    }
}

void EditorView::pasteClipboard() {
    const auto text = QApplication::clipboard()->text().toUtf8();
    const std::string_view utf8(text.constData(), static_cast<std::size_t>(text.size()));
    if (hasAnySelection()) (void)history_.replaceSelections(utf8);
    else (void)history_.insertText(utf8);
    afterEdit();
}

std::size_t EditorView::previousScalar(std::size_t offset) const {
    if (offset == 0) return 0;
    auto start = offset - 1;
    while (start > 0 && (buffer_.byteAt(start) & 0xc0) == 0x80) --start;
    return start;
}

std::size_t EditorView::nextScalar(std::size_t offset) const {
    if (offset >= buffer_.sizeBytes()) return buffer_.sizeBytes();
    auto end = offset + 1;
    while (end < buffer_.sizeBytes() && (buffer_.byteAt(end) & 0xc0) == 0x80) ++end;
    return end;
}

void EditorView::selectWord(std::size_t offset) {
    if (offset > 0 && offset == buffer_.sizeBytes()) offset = previousScalar(offset);
    if (offset == buffer_.sizeBytes()) {
        setPrimarySelection(offset, offset);
        return;
    }
    const auto isWord = [this](std::size_t position) {
        const auto byte = buffer_.byteAt(position);
        return byte >= 0x80 || std::isalnum(byte) || byte == '_';
    };
    if (!isWord(offset) && offset > 0) offset = previousScalar(offset);
    if (!isWord(offset)) {
        setPrimarySelection(offset, nextScalar(offset));
        return;
    }
    auto begin = offset;
    while (begin > 0) {
        const auto previous = previousScalar(begin);
        if (!isWord(previous)) break;
        begin = previous;
    }
    auto end = nextScalar(offset);
    while (end < buffer_.sizeBytes() && isWord(end)) end = nextScalar(end);
    setPrimarySelection(begin, end);
    cursors_.setPrimaryCursor(end);
    dragAnchor_ = begin;
    renderer_->viewport()->update();
}

bool EditorView::handleMousePress(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return false;
    renderer_->setFocus(Qt::MouseFocusReason);
    const auto offset = renderer_->offsetAtPosition(event->position());
    extendingSelection_ = event->modifiers().testFlag(Qt::ShiftModifier);
    dragAnchor_ = extendingSelection_ && selections_.selectionCount() > 0 &&
                          selections_.hasSelection(0)
                      ? selections_.selection(0).anchor
                      : extendingSelection_ ? cursors_.primaryOffset() : offset;
    cursors_.setPrimaryCursor(offset);
    if (extendingSelection_) setPrimarySelection(dragAnchor_, offset);
    else collapseSelectionsAt(offset);
    dragging_ = true;
    renderer_->viewport()->setCursor(Qt::IBeamCursor);
    renderer_->viewport()->update();
    return true;
}

bool EditorView::handleMouseMove(QMouseEvent* event) {
    if (!dragging_ || !(event->buttons() & Qt::LeftButton)) return false;
    const auto offset = renderer_->offsetAtPosition(event->position());
    cursors_.setPrimaryCursor(offset);
    setPrimarySelection(dragAnchor_, offset);
    renderer_->viewport()->update();
    return true;
}

bool EditorView::handleMouseRelease(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !dragging_) return false;
    dragging_ = false;
    extendingSelection_ = false;
    return true;
}

bool EditorView::handleMouseDoubleClick(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return false;
    const auto offset = renderer_->offsetAtPosition(event->position());
    selectWord(offset);
    dragging_ = true;
    return true;
}

} // namespace note::ui
