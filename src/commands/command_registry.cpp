#include "commands/command_registry.hpp"

#include <algorithm>
#include <cctype>

namespace note::commands {
namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
}

void CommandRegistry::registerCommand(Command command, Callback callback, Enabled enabled) {
    if (command.id.empty()) return;
    const auto id = command.id;
    entries_[id] = Entry{std::move(command), std::move(callback),
                                 std::move(enabled), true};
}

bool CommandRegistry::execute(const std::string& id) const {
    const auto it = entries_.find(id);
    if (it == entries_.end() || !isEnabled(id)) return false;
    if (it->second.callback) it->second.callback();
    return true;
}

bool CommandRegistry::contains(const std::string& id) const noexcept {
    return entries_.contains(id);
}

std::vector<Command> CommandRegistry::commands() const {
    std::vector<Command> result;
    result.reserve(entries_.size());
    for (const auto& [id, entry] : entries_) result.push_back(entry.command);
    std::sort(result.begin(), result.end(), [](const Command& a, const Command& b) {
        if (a.category != b.category) return a.category < b.category;
        return a.text < b.text;
    });
    return result;
}

std::vector<Command> CommandRegistry::search(const std::string& query) const {
    const auto needle = lower(query);
    std::vector<Command> result;
    for (const auto& command : commands()) {
        if (needle.empty() ||
            lower(command.text).find(needle) != std::string::npos ||
            lower(command.category).find(needle) != std::string::npos ||
            lower(command.description).find(needle) != std::string::npos ||
            lower(command.id).find(needle) != std::string::npos)
            result.push_back(command);
    }
    return result;
}

bool CommandRegistry::isEnabled(const std::string& id) const {
    const auto it = entries_.find(id);
    if (it == entries_.end() || !it->second.forcedEnabled) return false;
    return !it->second.enabled || it->second.enabled();
}

void CommandRegistry::setEnabled(const std::string& id, bool enabled) {
    const auto it = entries_.find(id);
    if (it != entries_.end()) it->second.forcedEnabled = enabled;
}

} // namespace note::commands
