#pragma once

#include "customisation/settings_manager.hpp"
#include "customisation/theme_manager.hpp"
#include "commands/command_registry.hpp"
#include "commands/keybinding_manager.hpp"

#include <QDialog>

#include <functional>

namespace note::ui {

class SettingsDialog final : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(customisation::SettingsManager& settings,
                   customisation::ThemeManager& themes,
                   commands::KeybindingManager& keybindings,
                   std::function<void()> applyCallback,
                   QWidget* parent = nullptr,
                   const commands::CommandRegistry* commandNames = nullptr);

private:
    void apply();
    customisation::SettingsManager& settings_;
    customisation::ThemeManager& themes_;
    std::function<void()> applyCallback_;
    commands::KeybindingManager& keybindings_;
};

} // namespace note::ui
