#include "ui/search_panel.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace note::ui {

SearchPanel::SearchPanel(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Find and Replace"));
    setObjectName(QStringLiteral("searchPanel"));
    auto* layout = new QVBoxLayout(this);
    findEdit_ = new QLineEdit(this);
    findEdit_->setPlaceholderText(tr("Find"));
    replaceEdit_ = new QLineEdit(this);
    replaceEdit_->setPlaceholderText(tr("Replace"));
    layout->addWidget(findEdit_);
    layout->addWidget(replaceEdit_);
    auto* options = new QHBoxLayout;
    caseSensitive_ = new QCheckBox(tr("Case sensitive"), this);
    wholeWord_ = new QCheckBox(tr("Whole word"), this);
    regex_ = new QCheckBox(tr("Regular expression"), this);
    options->addWidget(caseSensitive_);
    options->addWidget(wholeWord_);
    options->addWidget(regex_);
    layout->addLayout(options);
    auto* buttons = new QHBoxLayout;
    auto* previous = new QPushButton(tr("Previous"), this);
    auto* next = new QPushButton(tr("Next"), this);
    auto* replace = new QPushButton(tr("Replace"), this);
    auto* replaceAllButton = new QPushButton(tr("Replace All"), this);
    buttons->addWidget(previous);
    buttons->addWidget(next);
    buttons->addWidget(replace);
    buttons->addWidget(replaceAllButton);
    layout->addLayout(buttons);
    status_ = new QLabel(this);
    layout->addWidget(status_);
    connect(findEdit_, &QLineEdit::textChanged, this, [this] { runSearch(); });
    connect(caseSensitive_, &QCheckBox::toggled, this, [this] { runSearch(); });
    connect(wholeWord_, &QCheckBox::toggled, this, [this] { runSearch(); });
    connect(regex_, &QCheckBox::toggled, this, [this] { runSearch(); });
    connect(previous, &QPushButton::clicked, this, [this] { previousMatch(); });
    connect(next, &QPushButton::clicked, this, [this] { nextMatch(); });
    connect(replace, &QPushButton::clicked, this, [this] { replaceCurrent(); });
    connect(replaceAllButton, &QPushButton::clicked, this, [this] { replaceAll(); });
}

void SearchPanel::setActiveView(EditorView* view) {
    view_ = view;
    runSearch();
}

void SearchPanel::findNext() {
    if (matches_.empty()) runSearch();
    nextMatch();
}

void SearchPanel::findPrevious() {
    if (matches_.empty()) runSearch();
    previousMatch();
}

void SearchPanel::openFind() {
    replaceEdit_->hide();
    show();
    raise();
    activateWindow();
    findEdit_->setFocus();
    findEdit_->selectAll();
}

void SearchPanel::openReplace() {
    replaceEdit_->show();
    show();
    raise();
    activateWindow();
    findEdit_->setFocus();
    findEdit_->selectAll();
}

void SearchPanel::runSearch() {
    matches_.clear();
    current_ = 0;
    if (!view_) {
        status_->setText(tr("No active document"));
        return;
    }
    note::search::SearchOptions options;
    options.caseSensitive = caseSensitive_->isChecked();
    options.wholeWord = wholeWord_->isChecked();
    options.regularExpression = regex_->isChecked();
    const auto result = note::search::SearchEngine{}.find(
        view_->buffer(), findEdit_->text().toStdString(), options);
    matches_ = result.matches;
    if (result.status == note::search::SearchStatus::invalidPattern)
        status_->setText(tr("Invalid regular expression"));
    else if (result.status == note::search::SearchStatus::aborted)
        status_->setText(tr("Search aborted: pattern or size limit reached"));
    else
        status_->setText(tr("%1 match(es)").arg(static_cast<qulonglong>(matches_.size())));
    updateMatches();
}

void SearchPanel::updateMatches() {
    if (!view_) return;
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    ranges.reserve(matches_.size());
    for (const auto& match : matches_)
        ranges.emplace_back(match.begin, match.end);
    view_->setSearchMatches(ranges);
}

void SearchPanel::nextMatch() {
    if (matches_.empty()) return;
    current_ = matches_.size() == 1 ? 0 : (current_ + 1) % matches_.size();
    const auto& match = matches_[current_];
    view_->selectRange(match.begin, match.end);
}

void SearchPanel::previousMatch() {
    if (matches_.empty()) return;
    current_ = matches_.empty() ? 0
        : (current_ == 0 ? matches_.size() - 1 : current_ - 1);
    const auto& match = matches_[current_];
    view_->selectRange(match.begin, match.end);
}

void SearchPanel::replaceCurrent() {
    if (!view_ || matches_.empty()) return;
    if (current_ >= matches_.size()) current_ = 0;
    const auto match = matches_[current_];
    view_->replaceRange(match.begin, match.end - match.begin,
                        replaceEdit_->text().toStdString());
    runSearch();
}

void SearchPanel::replaceAll() {
    if (!view_ || matches_.empty()) return;
    view_->beginUndoTransaction();
    auto replacement = matches_;
    std::sort(replacement.begin(), replacement.end(),
              [](const note::search::SearchMatch& a,
                 const note::search::SearchMatch& b) {
                  return a.begin > b.begin;
              });
    for (const auto& match : replacement)
        view_->replaceRange(match.begin, match.end - match.begin,
                            replaceEdit_->text().toStdString());
    view_->endUndoTransaction();
    runSearch();
}

} // namespace note::ui
