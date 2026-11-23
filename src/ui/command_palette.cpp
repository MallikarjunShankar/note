#include "ui/command_palette.hpp"

#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace note::ui {

CommandPalette::CommandPalette(commands::CommandRegistry& registry,
                               QWidget* parent,
                               const commands::KeybindingManager* keybindings)
    : QDialog(parent), registry_(registry), keybindings_(keybindings) {
    setWindowTitle(tr("Command Palette"));
    setObjectName(QStringLiteral("commandPalette"));
    resize(520, 360);
    auto* layout = new QVBoxLayout(this);
    query_ = new QLineEdit(this);
    query_->setPlaceholderText(tr("Type a command"));
    query_->installEventFilter(this);
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
        auto label = QString::fromStdString(command.category + " — " + command.text);
        if (keybindings_) {
            const auto shortcut = keybindings_->binding(command.id)
                .toString(QKeySequence::NativeText);
            if (!shortcut.isEmpty()) label += QStringLiteral("    (%1)").arg(shortcut);
        }
        auto* item = new QListWidgetItem(label);
        item->setToolTip(QString::fromStdString(command.description));
        item->setStatusTip(item->toolTip());
        item->setData(Qt::UserRole, QString::fromStdString(command.id));
        results_->addItem(item);
    }
    if (results_->count() > 0) results_->setCurrentRow(0);
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event) {
    // Keep focus in the query while Up/Down choose a result (keyboard-only use).
    if (watched == query_ && event->type() == QEvent::KeyPress && results_->count() > 0) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Down || key == Qt::Key_Up) {
            const int step = key == Qt::Key_Down ? 1 : -1;
            const int count = results_->count();
            results_->setCurrentRow((results_->currentRow() + step + count) % count);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPalette::executeSelected() {
    const auto* item = results_->currentItem();
    if (!item) return;
    const auto id = item->data(Qt::UserRole).toString().toStdString();
    accept();
    registry_.execute(id);
}

} // namespace note::ui
