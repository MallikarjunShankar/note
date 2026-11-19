#include "editor/bracket_manager.hpp"
#include "editor/text_buffer.hpp"

#include <iostream>
#include <stdexcept>

using note::editor::BracketConfiguration;
using note::editor::BracketManager;
using note::editor::TextBuffer;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testPairConfiguration() {
    TextBuffer text("x");
    BracketManager brackets(text);
    for (const auto opening : {'(', '[', '{', '"', '\''})
        require(brackets.closingFor(opening).has_value(),
                "all specified pair types are enabled by default");

    auto configuration = brackets.configuration();
    configuration.squareBrackets = false;
    configuration.singleQuotes = false;
    brackets.setConfiguration(configuration);
    require(!brackets.closingFor('[') && !brackets.closingFor('\'') &&
                brackets.closingFor('(') == ')' &&
                !brackets.shouldPairAt(0, '['),
            "pair types can be configured independently");
}

void testQuoteContextAndUnicode() {
    TextBuffer text("λword\\");
    BracketManager brackets(text);
    require(!brackets.shouldPairAt(5, '\''),
            "quotes typed directly after a word byte are not doubled");
    require(!brackets.shouldPairAt(text.sizeBytes(), '"') &&
                brackets.shouldPairAt(0, '\'') &&
                brackets.shouldPairAt(5, '('),
            "quotes at word boundaries and ordinary brackets can pair");
    require(!brackets.shouldPairAt(text.sizeBytes(), '\''),
            "a quote escaped by an odd backslash is not auto-paired");

    TextBuffer unicode("λ");
    BracketManager unicodeBrackets(unicode);
    require(!unicodeBrackets.shouldPairAt(1, '"') &&
                !unicodeBrackets.shouldPairAt(2, '"') &&
                unicodeBrackets.shouldPairAt(2, '('),
            "UTF-8 scalar boundaries are respected around word characters");
}

void testNestedMatching() {
    TextBuffer text("([{}])");
    BracketManager brackets(text);
    const auto outerFromInside = brackets.matchingAtCursor(1);
    require(outerFromInside && outerFromInside->opening == 0 &&
                outerFromInside->closing == 5,
            "matching scans nested bracket types from the opening side");
    const auto nestedFromClose = brackets.matchingAtCursor(4);
    require(nestedFromClose && nestedFromClose->opening == 2 &&
                nestedFromClose->closing == 3,
            "matching scans nested bracket types from the closing side");

    TextBuffer malformed("([)]");
    BracketManager malformedBrackets(malformed);
    require(!malformedBrackets.matchingAtCursor(0),
            "mismatched nesting is not reported as a match");
}

void testEmptyPairDetectionAndInvalidOffsets() {
    TextBuffer text("()[]{}\"\"''");
    BracketManager brackets(text);
    const std::size_t caretPositions[]{1, 3, 5, 7, 9};
    for (const auto caret : caretPositions) {
        const auto pair = brackets.emptyPairAtCursor(caret);
        require(pair && pair->closing == pair->opening + 1,
                "every enabled bracket and quote pair is recognized");
    }
    require(!brackets.emptyPairAtCursor(0) &&
                !brackets.emptyPairAtCursor(text.sizeBytes()),
            "document boundaries are safe");

    auto configuration = BracketConfiguration{};
    configuration.parentheses = false;
    brackets.setConfiguration(configuration);
    require(!brackets.emptyPairAtCursor(1),
            "disabled pair types are not treated as auto-pairs");
}

} // namespace

int main() {
    try {
        testPairConfiguration();
        testQuoteContextAndUnicode();
        testNestedMatching();
        testEmptyPairDetectionAndInvalidOffsets();
        std::cout << "All bracket manager tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Bracket manager test failure: " << error.what() << '\n';
        return 1;
    }
}
