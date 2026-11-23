#pragma once

#include "search/search_engine.hpp"
#include "ui/editor_view.hpp"

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace note::ui {

class SearchPanel final : public QDialog {
    Q_OBJECT
public:
    explicit SearchPanel(QWidget* parent = nullptr);
    void setActiveView(EditorView* view);
    void openFind();
    void openReplace();

private:
    void runSearch();
    void nextMatch();
    void previousMatch();
    void replaceCurrent();
    void replaceAll();
    void updateMatches();

    EditorView* view_{};
    QLineEdit* findEdit_{};
    QLineEdit* replaceEdit_{};
    QCheckBox* caseSensitive_{};
    QCheckBox* wholeWord_{};
    QCheckBox* regex_{};
    QLabel* status_{};
    std::vector<note::search::SearchMatch> matches_;
    std::size_t current_{0};
};

} // namespace note::ui
