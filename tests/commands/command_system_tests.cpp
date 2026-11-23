#include "commands/command_registry.hpp"
#include "commands/keybinding_manager.hpp"

#include <QCoreApplication>
#include <QSettings>
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
    keys.setDefault("file.terminal", {});
    std::string conflict;
    require(keys.setBinding("edit.test",
                            QKeySequence(Qt::CTRL | Qt::Key_K), &conflict),
            "keybinding accepts a binding");
    require(keys.binding("edit.test") == QKeySequence(Qt::CTRL | Qt::Key_K),
            "keybinding resolves");
    require(keys.bindings().size() == 2 &&
                keys.setBinding("file.terminal",
                                QKeySequence(Qt::CTRL | Qt::Key_T), &conflict) &&
                keys.binding("file.terminal") ==
                    QKeySequence(Qt::CTRL | Qt::Key_T),
            "commands without defaults remain user-configurable");
    require(keys.setBinding("file.terminal", QKeySequence{}, &conflict) &&
                keys.binding("file.terminal").isEmpty(),
            "an explicit empty binding disables a shortcut");

    QSettings invalidOverrides(temporary.filePath("keybindings.ini"),
                               QSettings::IniFormat);
    invalidOverrides.setValue("keybindings/edit.test",
                              QKeySequence(Qt::CTRL | Qt::Key_T).toString());
    invalidOverrides.setValue("keybindings/file.terminal",
                              QKeySequence(Qt::CTRL | Qt::Key_T).toString());
    invalidOverrides.sync();
    note::commands::KeybindingManager restored(
        temporary.filePath("keybindings.ini"));
    restored.setDefault("edit.test", QKeySequence(Qt::CTRL | Qt::Key_K));
    restored.setDefault("file.terminal", {});
    restored.load();
    require(restored.binding("edit.test") ==
                QKeySequence(Qt::CTRL | Qt::Key_K) &&
                restored.binding("file.terminal").isEmpty(),
            "conflicting persisted overrides safely fall back to defaults");

    keys.restoreDefaults();
    require(keys.bindings().size() == 2 &&
                keys.binding("file.terminal").isEmpty(),
            "restore defaults retains command bindings");
    return 0;
}
