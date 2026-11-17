#pragma once

#include <QPlainTextEdit>

namespace note::documents { class Document; }

namespace note::ui {

// Temporary, read-only lifecycle preview. It displays a bounded projection of
// the active model and is not the production editor or a document owner.
class DocumentPreview final : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit DocumentPreview(QWidget* parent = nullptr);
    void showDocument(const documents::Document* document);

private:
    static constexpr std::size_t previewLimit = 16 * 1024;
};

} // namespace note::ui
