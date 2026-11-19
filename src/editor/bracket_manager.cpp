#include "editor/bracket_manager.hpp"

#include "editor/text_buffer.hpp"

#include <string_view>
#include <vector>

namespace note::editor {
namespace {

constexpr std::string_view kOpenings{"([{"};
constexpr std::string_view kClosings{")]}"};

} // namespace

BracketManager::BracketManager(const TextBuffer& buffer) : buffer_(buffer) {}

const BracketConfiguration& BracketManager::configuration() const noexcept {
    return configuration_;
}

void BracketManager::setConfiguration(BracketConfiguration configuration) noexcept {
    configuration_ = configuration;
}

std::optional<char> BracketManager::closingFor(char opening) const noexcept {
    switch (opening) {
    case '(':
        if (configuration_.parentheses) return ')';
        break;
    case '[':
        if (configuration_.squareBrackets) return ']';
        break;
    case '{':
        if (configuration_.curlyBraces) return '}';
        break;
    case '"':
        if (configuration_.doubleQuotes) return '"';
        break;
    case '\'':
        if (configuration_.singleQuotes) return '\'';
        break;
    default:
        break;
    }
    return std::nullopt;
}

bool BracketManager::isEnabledPair(char opening, char closing) const noexcept {
    const auto expected = closingFor(opening);
    return expected && *expected == closing;
}

bool BracketManager::shouldPairAt(std::size_t offset, char opening) const {
    if (!closingFor(opening) || offset > buffer_.sizeBytes() ||
        !buffer_.isScalarBoundary(offset))
        return false;
    if (opening == '"' || opening == '\'') {
        if (offset > 0) {
            const auto previous = previousBoundary(offset);
            if (isWordByte(buffer_.byteAt(previous))) return false;
        }
        auto position = offset;
        std::size_t backslashes = 0;
        while (position > 0) {
            const auto previous = previousBoundary(position);
            if (buffer_.byteAt(previous) != '\\') break;
            ++backslashes;
            position = previous;
        }
        if (backslashes % 2 != 0) return false;
    }
    return true;
}

std::optional<BracketPair> BracketManager::matchingAtCursor(
    std::size_t offset) const {
    if (offset > buffer_.sizeBytes() || !buffer_.isScalarBoundary(offset))
        return std::nullopt;
    if (offset > 0) {
        if (auto match = matchingAt(previousBoundary(offset))) return match;
    }
    return matchingAt(offset);
}

std::optional<BracketPair> BracketManager::emptyPairAtCursor(
    std::size_t offset) const {
    if (offset == 0 || offset >= buffer_.sizeBytes() ||
        !buffer_.isScalarBoundary(offset))
        return std::nullopt;
    const auto openingOffset = previousBoundary(offset);
    if (offset != openingOffset + 1) return std::nullopt;
    const auto opening = static_cast<char>(buffer_.byteAt(openingOffset));
    const auto closing = static_cast<char>(buffer_.byteAt(offset));
    if (isEnabledPair(opening, closing))
        return BracketPair{openingOffset, offset};
    return std::nullopt;
}

std::optional<BracketPair> BracketManager::matchingAt(
    std::size_t offset) const {
    if (offset >= buffer_.sizeBytes()) return std::nullopt;
    const auto current = static_cast<char>(buffer_.byteAt(offset));
    const auto openingPosition = kOpenings.find(current);
    const auto closingPosition = kClosings.find(current);
    if (openingPosition != std::string_view::npos) {
        std::vector<char> expectedClosings{ kClosings[openingPosition] };
        for (auto position = offset + 1; position < buffer_.sizeBytes(); ++position) {
            const auto value = static_cast<char>(buffer_.byteAt(position));
            const auto nestedOpening = kOpenings.find(value);
            if (nestedOpening != std::string_view::npos) {
                expectedClosings.push_back(kClosings[nestedOpening]);
                continue;
            }
            if (expectedClosings.empty() || value != expectedClosings.back())
                continue;
            expectedClosings.pop_back();
            if (expectedClosings.empty()) return BracketPair{offset, position};
        }
        return std::nullopt;
    }
    if (closingPosition != std::string_view::npos) {
        std::vector<char> expectedOpenings{ kOpenings[closingPosition] };
        auto position = offset;
        while (position > 0) {
            --position;
            const auto value = static_cast<char>(buffer_.byteAt(position));
            const auto nestedClosing = kClosings.find(value);
            if (nestedClosing != std::string_view::npos) {
                expectedOpenings.push_back(kOpenings[nestedClosing]);
                continue;
            }
            if (expectedOpenings.empty() || value != expectedOpenings.back())
                continue;
            expectedOpenings.pop_back();
            if (expectedOpenings.empty()) return BracketPair{position, offset};
        }
    }
    return std::nullopt;
}

std::size_t BracketManager::previousBoundary(std::size_t offset) const {
    --offset;
    while (offset > 0 && !buffer_.isScalarBoundary(offset)) --offset;
    return offset;
}

bool BracketManager::isWordByte(unsigned char value) noexcept {
    return value >= 0x80 || (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
           value == '_';
}

} // namespace note::editor
