#include "platform/path_resolver.hpp"

namespace note::platform {

QString pathToQString(const std::filesystem::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.native());
#else
    const auto value = path.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(value.data()),
                             static_cast<qsizetype>(value.size()));
#endif
}

std::filesystem::path pathFromQString(const QString& path) {
#if defined(_WIN32)
    return std::filesystem::path(path.toStdWString());
#else
    const auto bytes = path.toUtf8();
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t*>(bytes.constData()),
        static_cast<std::size_t>(bytes.size())));
#endif
}

} // namespace note::platform
