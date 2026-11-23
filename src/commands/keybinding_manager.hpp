#pragma once

#include <QKeySequence>
#include <QString>

#include <unordered_map>
#include <vector>

namespace note::commands {

struct Keybinding {
    std::string commandId;
    QKeySequence sequence;
};

class KeybindingManager final {
public:
    explicit KeybindingManager(QString storageFile);
    void setDefault(const std::string& commandId, const QKeySequence& sequence);
    bool setBinding(const std::string& commandId, const QKeySequence& sequence,
                    std::string* conflict = nullptr);
    void removeBinding(const std::string& commandId);
    void restoreDefaults();
    [[nodiscard]] QKeySequence binding(const std::string& commandId) const;
    [[nodiscard]] std::vector<Keybinding> bindings() const;
    void save();
    void load();

private:
    QString storageFile_;
    std::unordered_map<std::string, QKeySequence> defaults_;
    std::unordered_map<std::string, QKeySequence> overrides_;
};

} // namespace note::commands
