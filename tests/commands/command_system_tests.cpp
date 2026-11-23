#include "commands/command_registry.hpp"
#include "commands/keybinding_manager.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    note::commands::CommandRegistry registry;
    int executed = 0;
    registry.registerCommand({"edit.test", "Test", "Edit", "Test command"},
                             [&executed] { ++executed; });
    require(registry.execute("edit.test") && executed == 1,
            "command executes once");
    registry.setEnabled("edit.test", false);
    require(!registry.execute("edit.test") && executed == 1,
            "disabled command cannot execute");
    registry.setEnabled("edit.test", true);
    require(!registry.execute("missing.command"),
            "unknown command is rejected");

    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    note::commands::KeybindingManager keys(
        temporary.filePath("keybindings.ini"));
    keys.setDefault("edit.test", QKeySequence(Qt::CTRL | Qt::Key_K));
    std::string conflict;
    require(keys.setBinding("edit.test",
                            QKeySequence(Qt::CTRL | Qt::Key_K), &conflict),
            "keybinding accepts a binding");
    require(keys.binding("edit.test") == QKeySequence(Qt::CTRL | Qt::Key_K),
            "keybinding resolves");
    keys.restoreDefaults();
    require(keys.bindings().size() == 1,
            "restore defaults retains command bindings");
    return 0;
}
