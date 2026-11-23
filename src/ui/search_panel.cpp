#include "ui/search_panel.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
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
    continueTimer_.setSingleShot(true);
    continueTimer_.setInterval(0);
    connect(&continueTimer_, &QTimer::timeout, this, [this] { continueSearch(); });
}

void SearchPanel::setActiveView(EditorView* view) {
    if (replacing_) return; // replaceAll re-searches once when it finishes
    // Called on every edit: only re-search while the panel is visible; otherwise
    // drop results (offsets may be stale) and let findNext/open re-run them.
    if (!isVisible()) {
        continueTimer_.stop();
        searching_ = false;
        pendingStep_ = 0;
        if (view_ && !matches_.empty()) {
            matches_.clear();
            current_ = 0;
            view_->setSearchMatches({});
        }
    }
    view_ = view;
    if (isVisible()) runSearch();
}

void SearchPanel::findNext() {
    if (matches_.empty() && !searching_) runSearch();
    if (matches_.empty() && searching_) pendingStep_ = 1;
    else nextMatch();
}

void SearchPanel::findPrevious() {
    if (matches_.empty() && !searching_) runSearch();
    if (matches_.empty() && searching_) pendingStep_ = -1;
    else previousMatch();
}

void SearchPanel::openFind() {
    replaceEdit_->hide();
    show();
    raise();
    activateWindow();
    findEdit_->setFocus();
    findEdit_->selectAll();
    runSearch();
}

void SearchPanel::openReplace() {
    replaceEdit_->show();
    show();
    raise();
    activateWindow();
    findEdit_->setFocus();
    findEdit_->selectAll();
    runSearch();
}

void SearchPanel::runSearch() {
    continueTimer_.stop();
    matches_.clear();
    current_ = 0;
    searching_ = false;
    if (!view_) {
        pendingStep_ = 0;
        status_->setText(tr("No active document"));
        return;
    }
    query_ = findEdit_->text().toStdString();
    options_.caseSensitive = caseSensitive_->isChecked();
    options_.wholeWord = wholeWord_->isChecked();
    options_.regularExpression = regex_->isChecked();
    resumeOffset_ = 0;
    revision_ = view_->buffer().revision();
    searching_ = true;
    continueSearch();
}

void SearchPanel::continueSearch() {
    if (!view_ || !searching_) return;
    if (view_->buffer().revision() != revision_) {
        runSearch(); // offsets from earlier slices are stale
        return;
    }
    constexpr std::size_t maxMatches = 10000;
    // One slice per event-loop turn keeps typing and painting responsive.
    const auto result = note::search::SearchEngine{}.find(
        view_->buffer(), query_, options_, maxMatches - matches_.size(),
        resumeOffset_, std::chrono::milliseconds{10});
    matches_.insert(matches_.end(), result.matches.begin(), result.matches.end());
    resumeOffset_ = result.resumeOffset;
    searching_ = result.status == note::search::SearchStatus::incomplete;
    const auto count = static_cast<qulonglong>(matches_.size());
    if (result.status == note::search::SearchStatus::invalidPattern)
        status_->setText(tr("Invalid regular expression"));
    else if (result.status == note::search::SearchStatus::aborted)
        status_->setText(matches_.size() >= maxMatches
            ? tr("Showing the first %1 matches").arg(count)
            : tr("Search stopped: pattern too long or too slow (%1 match(es) so far)").arg(count));
    else if (searching_)
        status_->setText(tr("Searching... %1 match(es)").arg(count));
    else
        status_->setText(tr("%1 match(es)").arg(count));
    updateMatches();
    if (pendingStep_ != 0 && (!matches_.empty() || !searching_)) {
        const auto step = pendingStep_;
        pendingStep_ = 0;
        step > 0 ? nextMatch() : previousMatch();
    }
    if (searching_) continueTimer_.start();
}

void SearchPanel::finishSearch() {
    while (searching_) continueSearch();
    continueTimer_.stop();
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
    finishSearch(); // replace every match, not only those found so far
    if (!view_ || matches_.empty()) return;
    replacing_ = true;
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
    replacing_ = false;
    runSearch();
}

} // namespace note::ui
