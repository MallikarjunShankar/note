#pragma once

#include "commands/command_registry.hpp"
#include "commands/keybinding_manager.hpp"

#include <QDialog>

class QLineEdit;
class QListWidget;

namespace note::ui {

class CommandPalette final : public QDialog {
    Q_OBJECT
public:
    explicit CommandPalette(commands::CommandRegistry& registry,
                            QWidget* parent = nullptr,
                            const commands::KeybindingManager* keybindings = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void filter();
    void executeSelected();
    commands::CommandRegistry& registry_;
    const commands::KeybindingManager* keybindings_{};
    QLineEdit* query_{};
    QListWidget* results_{};
};

} // namespace note::ui
