#pragma once

#include "commands/command_registry.hpp"
#include "commands/keybinding_manager.hpp"

#include <QKeyEvent>

namespace note::commands {

class ShortcutDispatcher final {
public:
    ShortcutDispatcher(CommandRegistry& registry, KeybindingManager& keybindings);
    [[nodiscard]] bool dispatch(QKeyEvent* event);
    void setEnabled(bool enabled) noexcept;

private:
    CommandRegistry& registry_;
    KeybindingManager& keybindings_;
    bool enabled_{true};
};

} // namespace note::commands
