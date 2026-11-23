#include "ui/command_palette.hpp"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace note::ui {

CommandPalette::CommandPalette(commands::CommandRegistry& registry,
                               QWidget* parent)
    : QDialog(parent), registry_(registry) {
    setWindowTitle(tr("Command Palette"));
    setObjectName(QStringLiteral("commandPalette"));
    resize(520, 360);
    auto* layout = new QVBoxLayout(this);
    query_ = new QLineEdit(this);
    query_->setPlaceholderText(tr("Type a command"));
    results_ = new QListWidget(this);
    layout->addWidget(query_);
    layout->addWidget(results_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                         QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(query_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(results_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { executeSelected(); });
    connect(buttons, &QDialogButtonBox::accepted, this,
            [this] { executeSelected(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    filter();
}

void CommandPalette::filter() {
    results_->clear();
    for (const auto& command : registry_.search(query_->text().toStdString())) {
        if (!registry_.isEnabled(command.id)) continue;
        auto* item = new QListWidgetItem(
            QString::fromStdString(command.category + " — " + command.text));
        item->setData(Qt::UserRole, QString::fromStdString(command.id));
        results_->addItem(item);
    }
    if (results_->count() > 0) results_->setCurrentRow(0);
}

void CommandPalette::executeSelected() {
    const auto* item = results_->currentItem();
    if (!item) return;
    const auto id = item->data(Qt::UserRole).toString().toStdString();
    accept();
    registry_.execute(id);
}

} // namespace note::ui
