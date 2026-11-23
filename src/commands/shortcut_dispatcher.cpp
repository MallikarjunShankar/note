#include "commands/shortcut_dispatcher.hpp"

namespace note::commands {

ShortcutDispatcher::ShortcutDispatcher(CommandRegistry& registry,
                                       KeybindingManager& keybindings)
    : registry_(registry), keybindings_(keybindings) {}

bool ShortcutDispatcher::dispatch(QKeyEvent* event) {
    if (!enabled_ || !event) return false;
    const auto sequence = QKeySequence(event->keyCombination().toCombined());
    for (const auto& binding : keybindings_.bindings()) {
        if (!binding.sequence.isEmpty() && binding.sequence.matches(sequence) ==
                                               QKeySequence::ExactMatch)
            return registry_.execute(binding.commandId);
    }
    return false;
}

void ShortcutDispatcher::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
}

} // namespace note::commands
