#include "ui/editor_view.hpp"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <QWheelEvent>

#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace note::ui {

EditorView::EditorView(editor::TextBuffer& buffer,
                       editor::LineEnding preferredLineEnding,
                       QWidget* parent)
    : QWidget(parent), buffer_(buffer), cursors_(buffer), selections_(buffer),
      brackets_(buffer),
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
editor::BracketManager& EditorView::brackets() noexcept {
    lastMatchRevision_ = std::numeric_limits<std::uint64_t>::max();
    return brackets_;
}
EditorRenderer* EditorView::renderer() const noexcept { return renderer_; }

void EditorView::setDocumentChangedCallback(std::function<void()> callback) {
    documentChangedCallback_ = std::move(callback);
}

void EditorView::refreshFromModel() {
    cursors_.syncToBuffer();
    selections_.syncToBuffer();
    ensureSelectionAlignment();
    normalizeCursorSelections();
    renderer_->refreshFromModel();
    updateMatchingBrackets();
}

bool EditorView::undo() {
    if (!history_.undo()) return false;
    afterEdit();
    return true;
}

bool EditorView::redo() {
    if (!history_.redo()) return false;
    afterEdit();
    return true;
}

void EditorView::indentSelectedLines() {
    (void)history_.indentLines(indentationUsesTabs_, indentationWidth_);
    afterEdit();
}

void EditorView::outdentSelectedLines() {
    (void)history_.outdentLines(indentationWidth_);
    afterEdit();
}

void EditorView::beginUndoTransaction() {
    history_.beginTransaction();
}

void EditorView::endUndoTransaction() {
    history_.endTransaction();
}

void EditorView::replaceRange(std::size_t offset, std::size_t length,
                              std::string_view utf8Text) {
    (void)history_.replaceRange(offset, length, utf8Text, preferredLineEnding_);
    afterEdit();
}

void EditorView::selectRange(std::size_t begin, std::size_t end) {
    ensureSelectionAlignment();
    setPrimarySelection(begin, end);
    cursors_.setPrimaryCursor(end);
    renderer_->ensureCursorVisible(end);
    renderer_->viewport()->update();
}

void EditorView::setSearchMatches(
    const std::vector<std::pair<std::size_t, std::size_t>>& matches) {
    QVector<QPair<std::size_t, std::size_t>> ranges;
    ranges.reserve(static_cast<qsizetype>(matches.size()));
    for (const auto& match : matches)
        ranges.push_back({match.first, match.second});
    renderer_->setSearchMatches(ranges);
}

void EditorView::setBracketConfiguration(editor::BracketConfiguration configuration) {
    brackets_.setConfiguration(configuration);
}

void EditorView::restoreCursorPosition(std::size_t offset) {
    cursors_.setPrimaryCursor(offset);
    selections_.clearSelections();
    selections_.addSelection(cursors_.primaryOffset(), cursors_.primaryOffset());
    renderer_->ensureCursorVisible(cursors_.primaryOffset());
    renderer_->viewport()->update();
}

void EditorView::setIndentationConfiguration(bool useTabs, std::size_t width) {
    indentationUsesTabs_ = useTabs;
    indentationWidth_ = std::clamp<std::size_t>(width, 1, 16);
}

void EditorView::setShortcutHandler(std::function<bool(QKeyEvent*)> handler) {
    shortcutHandler_ = std::move(handler);
}

bool EditorView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress)
        return handleKeyPress(static_cast<QKeyEvent*>(event));
    if (watched == renderer_->viewport()) {
        if (event->type() == QEvent::ContextMenu) {
            const auto* context = static_cast<QContextMenuEvent*>(event);
            emit contextMenuRequested(context->globalPos());
            return true;
        }
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
    if (shortcutHandler_ && shortcutHandler_(event)) return true;
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
    if (!shortcutHandler_ && event->matches(QKeySequence::Undo)) {
        if (history_.undo()) afterEdit();
        return true;
    }
    if (!shortcutHandler_ && event->matches(QKeySequence::Redo)) {
        if (history_.redo()) afterEdit();
        return true;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        cursors_.clearSecondaryCursors();
        setPrimarySelection(0, buffer_.sizeBytes());
        renderer_->viewport()->update();
        updateMatchingBrackets(true);
        return true;
    }

    const auto modifiers = event->modifiers();
    if (modifiers.testFlag(Qt::ControlModifier) &&
        !modifiers.testFlag(Qt::AltModifier) && !modifiers.testFlag(Qt::MetaModifier) &&
        (event->key() == Qt::Key_Home || event->key() == Qt::Key_End)) {
        moveCursor(event->key() == Qt::Key_Home ? documentStartKey : documentEndKey,
                   modifiers.testFlag(Qt::ShiftModifier));
        return true;
    }
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
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        moveCursor(event->key(), shift);
        return true;
    case Qt::Key_Backspace:
        backspaceWithPairHandling();
        afterEdit();
        return true;
    case Qt::Key_Delete:
        (void)history_.deleteForward();
        afterEdit();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        insertNewlineWithIndent();
        afterEdit();
        return true;
    case Qt::Key_Backtab: // Qt reports Shift+Tab as Backtab on real keyboards.
        outdentSelectedLines();
        return true;
    case Qt::Key_Tab:
        if (shift) outdentSelectedLines();
        else indentSelectedLines();
        return true;
    default:
        break;
    }

    const auto text = event->text();
    if (!text.isEmpty() && text.at(0).isPrint()) {
        const auto utf8 = text.toUtf8();
        if (utf8 == "}" && dedentClosingBrace()) {
            afterEdit();
            return true;
        }
        if (utf8.size() == 1 && handleTextCharacter(utf8.at(0))) {
            afterEdit();
            return true;
        }
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
    ensureSelectionAlignment();
    std::vector<std::size_t> anchors;
    anchors.reserve(cursors_.cursorCount());
    for (std::size_t index = 0; index < cursors_.cursorCount(); ++index)
        anchors.push_back(selections_.selection(index).anchor);

    switch (key) {
    case Qt::Key_Left: cursors_.moveLeft(); break;
    case Qt::Key_Right: cursors_.moveRight(); break;
    case Qt::Key_Up: cursors_.moveUp(); break;
    case Qt::Key_Down: cursors_.moveDown(); break;
    case Qt::Key_PageUp:
    case Qt::Key_PageDown: {
        const auto rows = std::max(1, renderer_->viewport()->height() /
                                          std::max(1, renderer_->configuration().lineHeight) - 1);
        for (int row = 0; row < rows; ++row)
            key == Qt::Key_PageUp ? cursors_.moveUp() : cursors_.moveDown();
        break;
    }
    case documentStartKey:
    case documentEndKey:
        for (std::size_t index = 0; index < cursors_.cursorCount(); ++index)
            setCursorOffset(index, key == documentStartKey ? 0 : buffer_.sizeBytes());
        break;
    case Qt::Key_Home:
    case Qt::Key_End:
        for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
            const auto location = cursors_.lineAndColumn(cursorOffset(index));
            const auto line = buffer_.lineRange(location.line);
            setCursorOffset(index, key == Qt::Key_Home
                                       ? line.begin
                                       : line.end);
        }
        break;
    default: break;
    }

    if (!extendSelection && (key == Qt::Key_Left || key == Qt::Key_Right)) {
        for (std::size_t index = 0; index < selections_.selectionCount(); ++index) {
            if (!selections_.hasSelection(index)) continue;
            const auto range = selections_.normalizedRange(index);
            setCursorOffset(index, key == Qt::Key_Left ? range.begin : range.end);
        }
    }
    ensureSelectionAlignment();
    if (extendSelection) {
        for (std::size_t index = 0; index < cursors_.cursorCount(); ++index)
            selections_.setSelection(index, anchors[index], cursorOffset(index));
    } else {
        collapseSelectionsAtCursors();
    }
    normalizeCursorSelections();
    renderer_->ensureCursorVisible(cursors_.primaryOffset());
    renderer_->viewport()->update();
    updateMatchingBrackets();
}

void EditorView::setPrimarySelection(std::size_t anchor, std::size_t active) {
    ensureSelectionAlignment();
    selections_.setSelection(0, anchor, active);
}

void EditorView::collapseSelectionsAt(std::size_t offset) {
    cursors_.setPrimaryCursor(offset);
    collapseSelectionsAtCursors();
}

void EditorView::collapseSelectionsAtCursors() {
    selections_.clearSelections();
    for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
        const auto offset = cursorOffset(index);
        selections_.addSelection(offset, offset);
    }
}

void EditorView::ensureSelectionAlignment() {
    while (selections_.selectionCount() > cursors_.cursorCount())
        (void)selections_.removeSelection(selections_.selectionCount() - 1);
    while (selections_.selectionCount() < cursors_.cursorCount()) {
        const auto index = selections_.selectionCount();
        const auto offset = cursorOffset(index);
        selections_.addSelection(offset, offset);
    }
}

void EditorView::normalizeCursorSelections() {
    ensureSelectionAlignment();
    std::size_t secondary = 0;
    while (secondary < cursors_.secondaryCursorCount()) {
        const auto selectionIndex = secondary + 1;
        const auto currentOffset = cursorOffset(selectionIndex);
        const auto currentRange = selections_.normalizedRange(selectionIndex);
        bool conflict = false;
        for (std::size_t prior = 0; prior < selectionIndex; ++prior) {
            if (cursorOffset(prior) == currentOffset) {
                conflict = true;
                break;
            }
            const auto priorRange = selections_.normalizedRange(prior);
            const bool currentEmpty = currentRange.begin == currentRange.end;
            const bool priorEmpty = priorRange.begin == priorRange.end;
            if ((!currentEmpty && !priorEmpty &&
                 currentRange.begin < priorRange.end &&
                 priorRange.begin < currentRange.end) ||
                (currentEmpty && !priorEmpty &&
                 currentRange.begin >= priorRange.begin &&
                 currentRange.begin <= priorRange.end) ||
                (!currentEmpty && priorEmpty &&
                 priorRange.begin >= currentRange.begin &&
                 priorRange.begin <= currentRange.end)) {
                conflict = true;
                break;
            }
        }
        if (!conflict) {
            ++secondary;
            continue;
        }
        (void)cursors_.removeSecondaryCursor(secondary);
        (void)selections_.removeSelection(selectionIndex);
    }
}

void EditorView::setCursorOffset(std::size_t index, std::size_t offset) {
    if (index == 0) cursors_.setPrimaryCursor(offset);
    else cursors_.setSecondaryCursor(index - 1, offset);
}

std::size_t EditorView::cursorOffset(std::size_t index) const {
    return index == 0 ? cursors_.primaryOffset()
                      : cursors_.secondaryCursorOffset(index - 1);
}

bool EditorView::hasAnySelection() const {
    for (std::size_t index = 0; index < selections_.selectionCount(); ++index)
        if (selections_.hasSelection(index)) return true;
    return false;
}

void EditorView::afterEdit() {
    ensureSelectionAlignment();
    normalizeCursorSelections();
    renderer_->refreshFromModel();
    renderer_->ensureCursorVisible(cursors_.primaryOffset());
    updateMatchingBrackets(true);
    if (documentChangedCallback_) documentChangedCallback_();
}

void EditorView::updateMatchingBrackets(bool force) {
    const auto offset = cursors_.primaryOffset();
    const auto revision = buffer_.revision();
    if (!force && offset == lastMatchCursor_ && revision == lastMatchRevision_)
        return;
    lastMatchCursor_ = offset;
    lastMatchRevision_ = revision;
    renderer_->setMatchingBrackets(brackets_.matchingAtCursor(offset));
}

bool EditorView::handleTextCharacter(char character) {
    ensureSelectionAlignment();
    const auto closing = brackets_.closingFor(character);
    const auto isClosing = character == ')' || character == ']' ||
                           character == '}' || character == '"' ||
                           character == '\'';
    if (!closing && !isClosing) return false;

    std::vector<bool> selected(cursors_.cursorCount());
    bool hasSelection = false;
    bool needsBracketBehavior = false;
    for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
        selected[index] = selections_.hasSelection(index);
        hasSelection = hasSelection || selected[index];
        if (selected[index]) continue;
        const auto offset = cursorOffset(index);
        const bool skipExisting =
            isClosing && offset < buffer_.sizeBytes() &&
            buffer_.byteAt(offset) == static_cast<unsigned char>(character);
        const bool pairOpening =
            closing && brackets_.shouldPairAt(offset, character);
        needsBracketBehavior = needsBracketBehavior || skipExisting || pairOpening;
    }
    if (!needsBracketBehavior) return false;

    history_.beginTransaction();
    if (hasSelection) (void)history_.deleteSelections();

    std::vector<std::size_t> skipped;
    std::vector<std::size_t> paired;
    std::vector<std::size_t> withExistingCloser;
    std::vector<std::size_t> insertedAt;
    skipped.reserve(cursors_.cursorCount());
    paired.reserve(cursors_.cursorCount());
    withExistingCloser.reserve(cursors_.cursorCount());
    insertedAt.reserve(cursors_.cursorCount());

    for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
        if (selected[index]) {
            insertedAt.push_back(index);
            continue;
        }
        const auto offset = cursorOffset(index);
        const bool skipExisting =
            isClosing && offset < buffer_.sizeBytes() &&
            buffer_.byteAt(offset) == static_cast<unsigned char>(character);
        if (skipExisting) {
            skipped.push_back(index);
            continue;
        }
        insertedAt.push_back(index);
        if (!closing || !brackets_.shouldPairAt(offset, character)) continue;
        paired.push_back(index);
        if (offset < buffer_.sizeBytes() &&
            buffer_.byteAt(offset) == static_cast<unsigned char>(*closing))
            withExistingCloser.push_back(index);
    }

    const auto text = std::string(1, character);
    if (paired.empty() && skipped.size() == cursors_.cursorCount()) {
        for (const auto index : skipped)
            setCursorOffset(index, cursorOffset(index) + 1);
        collapseSelectionsAtCursors();
        history_.endTransaction();
        return true;
    }

    if (!insertedAt.empty()) {
        (void)history_.insertTextAtCursorIndices(insertedAt, text);
        if (paired.empty()) {
            for (const auto index : skipped)
                setCursorOffset(index, cursorOffset(index) + 1);
        } else {
            std::vector<std::size_t> closingIndexes;
            closingIndexes.reserve(paired.size());
            for (const auto index : paired) {
                if (std::find(withExistingCloser.begin(),
                              withExistingCloser.end(), index) ==
                    withExistingCloser.end())
                    closingIndexes.push_back(index);
            }
            if (!closingIndexes.empty()) {
                const std::string closingText(1, *closing);
                (void)history_.insertTextAtCursorIndices(closingIndexes,
                                                         closingText);
                for (const auto index : closingIndexes)
                    setCursorOffset(index, cursorOffset(index) - 1);
            }
            for (const auto index : skipped)
                setCursorOffset(index, cursorOffset(index) + 1);
        }
    } else {
        for (const auto index : skipped)
            setCursorOffset(index, cursorOffset(index) + 1);
    }
    collapseSelectionsAtCursors();
    history_.endTransaction();
    return true;
}

void EditorView::backspaceWithPairHandling() {
    normalizeCursorSelections();
    std::vector<std::optional<editor::BracketPair>> pairs;
    for (;;) {
        pairs.assign(cursors_.cursorCount(), std::nullopt);
        std::vector<std::optional<std::pair<std::size_t, std::size_t>>> ranges(
            cursors_.cursorCount());
        for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
            if (selections_.hasSelection(index)) {
                const auto range = selections_.normalizedRange(index);
                ranges[index] = std::pair{range.begin, range.end};
                continue;
            }
            const auto offset = cursorOffset(index);
            pairs[index] = brackets_.emptyPairAtCursor(offset);
            if (pairs[index]) {
                ranges[index] = std::pair{pairs[index]->opening,
                                          pairs[index]->closing + 1};
            } else if (offset > 0) {
                ranges[index] = std::pair{previousScalar(offset), offset};
            }
        }

        bool removedConflict = false;
        for (std::size_t index = 1; index < ranges.size(); ++index) {
            if (!ranges[index]) continue;
            for (std::size_t prior = 0; prior < index; ++prior) {
                if (!ranges[prior] ||
                    ranges[index]->first >= ranges[prior]->second ||
                    ranges[prior]->first >= ranges[index]->second)
                    continue;
                (void)cursors_.removeSecondaryCursor(index - 1);
                (void)selections_.removeSelection(index);
                removedConflict = true;
                break;
            }
            if (removedConflict) break;
        }
        if (!removedConflict) break;
    }

    bool hasEmptyPair = false;
    for (std::size_t index = 0; index < cursors_.cursorCount(); ++index) {
        if (selections_.hasSelection(index)) pairs[index].reset();
        hasEmptyPair = hasEmptyPair || pairs[index].has_value();
    }
    if (!hasEmptyPair) {
        (void)history_.backspace();
        return;
    }
    history_.beginTransaction();
    ensureSelectionAlignment();
    for (std::size_t index = 0; index < pairs.size(); ++index) {
        if (!pairs[index]) continue;
        selections_.setSelection(index, pairs[index]->opening,
                                 pairs[index]->closing + 1);
    }
    (void)history_.backspace();
    history_.endTransaction();
}

bool EditorView::dedentClosingBrace() {
    if (cursors_.cursorCount() != 1 || hasAnySelection()) return false;
    const auto cursor = cursors_.primaryOffset();
    // An existing } after the cursor is skipped by pairing instead.
    if (cursor < buffer_.sizeBytes() && buffer_.byteAt(cursor) == '}') return false;
    const auto line = buffer_.lineRange(cursors_.primaryLocation().line);
    if (cursor == line.begin) return false;
    for (auto at = line.begin; at < cursor; ++at) {
        const auto byte = buffer_.byteAt(at);
        if (byte != ' ' && byte != '\t') return false;
    }
    auto cut = cursor;
    if (buffer_.byteAt(cut - 1) == '\t') {
        --cut;
    } else {
        for (std::size_t removed = 0; removed < indentationWidth_ && cut > line.begin &&
                                      buffer_.byteAt(cut - 1) == ' '; ++removed)
            --cut;
    }
    (void)history_.replaceRange(cut, cursor - cut, "}", preferredLineEnding_);
    setCursorOffset(0, cut + 1);
    collapseSelectionsAtCursors();
    return true;
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
        indentation.append(indentationUsesTabs_ ? "\t"
            : std::string(indentationWidth_, ' '));
    const auto indent = indentation.size();
    const auto inner = indentation;
    const auto afterCursor = cursor < buffer_.sizeBytes()
                                 ? buffer_.byteAt(cursor)
                                 : static_cast<unsigned char>(0);
    const bool betweenBraces = afterCursor == '}' && cursor > lineRange.begin &&
                               buffer_.byteAt(cursor - 1) == '{';
    if (afterCursor == '}' && indent != 0) {
        if (indentationUsesTabs_ && indentation.back() == '\t') {
            indentation.pop_back();
        } else {
            std::size_t remove = 0;
            while (remove < indentationWidth_ && !indentation.empty() &&
                   indentation.back() == ' ') {
                indentation.pop_back();
                ++remove;
            }
            if (remove == 0 && !indentation.empty() &&
                indentation.back() == '\t')
                indentation.pop_back();
        }
    }
    if (betweenBraces && cursors_.cursorCount() == 1 && !hasAnySelection()) {
        // {|} opens into a body line one level in, with } at the outer indent.
        (void)history_.insertText("\n" + inner + "\n" + indentation, preferredLineEnding_);
        setCursorOffset(0, cursor + 1 + inner.size());
        collapseSelectionsAtCursors();
        return;
    }
    std::string insertion{"\n"};
    insertion.append(indentation);
    if (hasAnySelection())
        (void)history_.replaceSelections(insertion, preferredLineEnding_);
    else
        (void)history_.insertText(insertion, preferredLineEnding_);
}

void EditorView::copySelection(bool cut) {
    if (!hasAnySelection()) return;
    std::string bytes;
    bool first = true;
    for (std::size_t index = 0; index < selections_.selectionCount(); ++index) {
        if (!selections_.hasSelection(index)) continue;
        if (!first) bytes.push_back('\n');
        const auto range = selections_.normalizedRange(index);
        bytes.append(buffer_.textRange(range.begin, range.end - range.begin));
        first = false;
    }
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
        cursors_.setPrimaryCursor(offset);
        dragAnchor_ = offset;
        normalizeCursorSelections();
        renderer_->viewport()->update();
        updateMatchingBrackets();
        return;
    }
    const auto isWord = [this](std::size_t position) {
        const auto byte = buffer_.byteAt(position);
        return byte >= 0x80 || std::isalnum(byte) || byte == '_';
    };
    if (!isWord(offset) && offset > 0) offset = previousScalar(offset);
    if (!isWord(offset)) {
        const auto end = nextScalar(offset);
        setPrimarySelection(offset, end);
        cursors_.setPrimaryCursor(end);
        dragAnchor_ = offset;
        normalizeCursorSelections();
        renderer_->viewport()->update();
        updateMatchingBrackets();
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
    normalizeCursorSelections();
    renderer_->viewport()->update();
    updateMatchingBrackets();
}

bool EditorView::handleMousePress(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return false;
    renderer_->setFocus(Qt::MouseFocusReason);
    const auto offset = renderer_->offsetAtPosition(event->position());
    if (event->modifiers().testFlag(Qt::AltModifier)) {
        dragging_ = false;
        extendingSelection_ = false;
        for (std::size_t index = 0; index < cursors_.secondaryCursorCount();
             ++index) {
            if (cursors_.secondaryCursorOffset(index) != offset) continue;
            (void)cursors_.removeSecondaryCursor(index);
            (void)selections_.removeSelection(index + 1);
            renderer_->viewport()->update();
            updateMatchingBrackets(true);
            return true;
        }
        for (std::size_t index = 0; index < selections_.selectionCount(); ++index) {
            if (!selections_.hasSelection(index)) continue;
            const auto range = selections_.normalizedRange(index);
            if (offset >= range.begin && offset <= range.end) return true;
        }
        if (cursors_.addSecondaryCursor(offset)) {
            selections_.addSelection(offset, offset);
            normalizeCursorSelections();
            renderer_->viewport()->update();
        }
        updateMatchingBrackets(true);
        return true;
    }
    extendingSelection_ = event->modifiers().testFlag(Qt::ShiftModifier);
    dragAnchor_ = extendingSelection_ && selections_.selectionCount() > 0 &&
                          selections_.hasSelection(0)
                      ? selections_.selection(0).anchor
                      : extendingSelection_ ? cursors_.primaryOffset() : offset;
    if (!extendingSelection_) cursors_.clearSecondaryCursors();
    cursors_.setPrimaryCursor(offset);
    if (extendingSelection_) setPrimarySelection(dragAnchor_, offset);
    else collapseSelectionsAt(offset);
    normalizeCursorSelections();
    dragging_ = true;
    renderer_->viewport()->setCursor(Qt::IBeamCursor);
    renderer_->viewport()->update();
    updateMatchingBrackets();
    return true;
}

bool EditorView::handleMouseMove(QMouseEvent* event) {
    if (!dragging_ || !(event->buttons() & Qt::LeftButton)) return false;
    const auto offset = renderer_->offsetAtPosition(event->position());
    cursors_.setPrimaryCursor(offset);
    setPrimarySelection(dragAnchor_, offset);
    normalizeCursorSelections();
    renderer_->viewport()->update();
    updateMatchingBrackets();
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
