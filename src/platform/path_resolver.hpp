#pragma once

#include <QString>

#include <filesystem>

namespace note::platform {

[[nodiscard]] QString pathToQString(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path pathFromQString(const QString& path);

} // namespace note::platform
