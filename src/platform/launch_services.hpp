#pragma once

#include <filesystem>
#include <string>

namespace note::platform {

struct LaunchResult {
    bool success{};
    std::string error;
};

[[nodiscard]] LaunchResult openInExternalApplication(const std::filesystem::path& file);
[[nodiscard]] LaunchResult openTerminalInDirectory(const std::filesystem::path& directory);

} // namespace note::platform
