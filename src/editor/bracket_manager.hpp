#pragma once

#include <cstddef>
#include <optional>

namespace note::editor {

class TextBuffer;

struct BracketPair {
    std::size_t opening{};
    std::size_t closing{};
    friend bool operator==(const BracketPair&, const BracketPair&) = default;
};

struct BracketConfiguration {
    bool parentheses{true};
    bool squareBrackets{true};
    bool curlyBraces{true};
    bool doubleQuotes{true};
    bool singleQuotes{true};
};

class BracketManager final {
public:
    explicit BracketManager(const TextBuffer& buffer);

    [[nodiscard]] const BracketConfiguration& configuration() const noexcept;
    void setConfiguration(BracketConfiguration configuration) noexcept;
    [[nodiscard]] std::optional<char> closingFor(char opening) const noexcept;
    [[nodiscard]] bool isEnabledPair(char opening, char closing) const noexcept;
    [[nodiscard]] bool shouldPairAt(std::size_t offset, char opening) const;
    [[nodiscard]] std::optional<BracketPair> matchingAtCursor(
        std::size_t offset) const;
    [[nodiscard]] std::optional<BracketPair> emptyPairAtCursor(
        std::size_t offset) const;

private:
    [[nodiscard]] std::optional<BracketPair> matchingAt(
        std::size_t offset) const;
    [[nodiscard]] std::size_t previousBoundary(std::size_t offset) const;
    [[nodiscard]] static bool isWordByte(unsigned char value) noexcept;

    const TextBuffer& buffer_;
    BracketConfiguration configuration_;
};

} // namespace note::editor
