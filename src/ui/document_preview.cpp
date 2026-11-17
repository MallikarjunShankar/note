#include "ui/document_preview.hpp"

#include "documents/document.hpp"

#include <QFontDatabase>
#include <QString>

#include <algorithm>
#include <stdexcept>

namespace note::ui {

DocumentPreview::DocumentPreview(QWidget* parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setPlaceholderText(tr("Create a document or open a file to get started."));
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setAccessibleName(tr("Read-only document preview"));
}

void DocumentPreview::showDocument(const documents::Document* document) {
    if (!document) {
        clear();
        setPlaceholderText(tr("Create a document or open a file to get started."));
        return;
    }

    const auto& buffer = document->buffer();
    auto length = std::min(buffer.sizeBytes(), previewLimit);
    if (length < buffer.sizeBytes()) {
        while (length > 0) {
            try {
                (void)buffer.textRange(0, length);
                break;
            } catch (const std::invalid_argument&) {
                --length;
            }
        }
    }

    const auto bytes = buffer.textRange(0, length);
    auto rendered = QString::fromUtf8(bytes.data(), static_cast<qsizetype>(bytes.size()));
    if (length < buffer.sizeBytes()) rendered += tr("\n\n[Preview truncated; document remains intact.] ");
    setPlaceholderText({});
    setPlainText(rendered);
    moveCursor(QTextCursor::Start);
}

} // namespace note::ui
