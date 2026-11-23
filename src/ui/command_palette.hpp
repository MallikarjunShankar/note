#pragma once

#include "commands/command_registry.hpp"

#include <QDialog>

class QLineEdit;
class QListWidget;

namespace note::ui {

class CommandPalette final : public QDialog {
    Q_OBJECT
public:
    explicit CommandPalette(commands::CommandRegistry& registry,
                            QWidget* parent = nullptr);

private:
    void filter();
    void executeSelected();
    commands::CommandRegistry& registry_;
    QLineEdit* query_{};
    QListWidget* results_{};
};

} // namespace note::ui
