#pragma once

#include "search/search_engine.hpp"
#include "ui/editor_view.hpp"

#include <QDialog>
#include <QPointer>
#include <QTimer>

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
    void findNext();
    void findPrevious();

private:
    void runSearch();
    void continueSearch();
    void finishSearch();
    void nextMatch();
    void previousMatch();
    void replaceCurrent();
    void replaceAll();
    void updateMatches();

    QPointer<EditorView> view_;
    QLineEdit* findEdit_{};
    QLineEdit* replaceEdit_{};
    QCheckBox* caseSensitive_{};
    QCheckBox* wholeWord_{};
    QCheckBox* regex_{};
    QLabel* status_{};
    std::vector<note::search::SearchMatch> matches_;
    std::size_t current_{0};
    // Large documents are searched in short slices on the UI thread.
    QTimer continueTimer_;
    std::string query_;
    note::search::SearchOptions options_;
    std::size_t resumeOffset_{};
    std::uint64_t revision_{};
    bool searching_{};
    bool replacing_{};
    int pendingStep_{}; // +1/-1: Find Next/Previous waiting for a first match.
};

} // namespace note::ui
