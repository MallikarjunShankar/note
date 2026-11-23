#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace note::commands {

struct Command {
    std::string id;
    std::string text;
    std::string category;
    std::string description;
};

class CommandRegistry final {
public:
    using Callback = std::function<void()>;
    using Enabled = std::function<bool()>;

    void registerCommand(Command command, Callback callback, Enabled enabled = {});
    bool execute(const std::string& id) const;
    [[nodiscard]] bool contains(const std::string& id) const noexcept;
    [[nodiscard]] std::vector<Command> commands() const;
    [[nodiscard]] std::vector<Command> search(const std::string& query) const;
    [[nodiscard]] bool isEnabled(const std::string& id) const;
    void setEnabled(const std::string& id, bool enabled);

private:
    struct Entry {
        Command command;
        Callback callback;
        Enabled enabled;
        bool forcedEnabled{true};
    };
    std::unordered_map<std::string, Entry> entries_;
};

class CommandExecutor final {
public:
    explicit CommandExecutor(const CommandRegistry& registry) : registry_(registry) {}
    [[nodiscard]] bool execute(const std::string& id) const { return registry_.execute(id); }
private:
    const CommandRegistry& registry_;
};

} // namespace note::commands
