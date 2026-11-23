#include "commands/keybinding_manager.hpp"

#include <QSettings>

#include <algorithm>
#include <set>

namespace note::commands {

KeybindingManager::KeybindingManager(QString storageFile)
    : storageFile_(std::move(storageFile)) {}

void KeybindingManager::setDefault(const std::string& commandId,
                                   const QKeySequence& sequence) {
    defaults_[commandId] = sequence;
}

bool KeybindingManager::setBinding(const std::string& commandId,
                                   const QKeySequence& sequence,
                                   std::string* conflict) {
    if (sequence.isEmpty()) {
        if (!defaults_.contains(commandId)) return false;
        overrides_[commandId] = QKeySequence{};
        save();
        return true;
    }
    for (const auto& [id, existing] : bindings()) {
        if (id == commandId) continue;
        if (!existing.isEmpty() && existing.matches(sequence) != QKeySequence::NoMatch) {
            if (conflict) *conflict = id;
            return false;
        }
    }
    overrides_[commandId] = sequence;
    save();
    return true;
}

void KeybindingManager::removeBinding(const std::string& commandId) {
    overrides_.erase(commandId);
    save();
}

void KeybindingManager::restoreDefaults() {
    overrides_.clear();
    save();
}

QKeySequence KeybindingManager::binding(const std::string& commandId) const {
    const auto override = overrides_.find(commandId);
    if (override != overrides_.end()) return override->second;
    const auto defaultBinding = defaults_.find(commandId);
    return defaultBinding == defaults_.end() ? QKeySequence{} : defaultBinding->second;
}

std::vector<Keybinding> KeybindingManager::bindings() const {
    std::vector<Keybinding> result;
    for (const auto& [id, sequence] : defaults_)
        result.push_back({id, binding(id)});
    std::sort(result.begin(), result.end(),
              [](const Keybinding& a, const Keybinding& b) {
                  return a.commandId < b.commandId;
              });
    return result;
}

void KeybindingManager::save() {
    QSettings settings(storageFile_, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("keybindings"));
    settings.remove(QString());
    for (const auto& [id, sequence] : overrides_)
        settings.setValue(QString::fromStdString(id), sequence.toString());
    settings.endGroup();
    settings.sync();
}

void KeybindingManager::load() {
    overrides_.clear();
    QSettings settings(storageFile_, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) return;
    settings.beginGroup(QStringLiteral("keybindings"));
    auto keys = settings.allKeys();
    std::sort(keys.begin(), keys.end());
    for (const auto& key : keys) {
        const auto id = key.toStdString();
        if (defaults_.contains(id))
            overrides_[id] = QKeySequence(settings.value(key).toString());
    }
    settings.endGroup();

    std::set<std::string> conflictingOverrides;
    const auto loaded = bindings();
    for (std::size_t first = 0; first < loaded.size(); ++first) {
        if (loaded[first].sequence.isEmpty()) continue;
        for (std::size_t second = first + 1; second < loaded.size(); ++second) {
            if (loaded[second].sequence.isEmpty() ||
                loaded[first].sequence.matches(loaded[second].sequence) ==
                    QKeySequence::NoMatch)
                continue;
            if (overrides_.contains(loaded[first].commandId))
                conflictingOverrides.insert(loaded[first].commandId);
            if (overrides_.contains(loaded[second].commandId))
                conflictingOverrides.insert(loaded[second].commandId);
        }
    }
    for (const auto& id : conflictingOverrides) overrides_.erase(id);
}

} // namespace note::commands
