#include "search/search_engine.hpp"

#include <stdexcept>
#include <string>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testChunkBoundaryInsideScalar() {
    // The second window starts at chunkBytesLimit - 512; put a 2-byte scalar across it
    // so a byte-sliced window would begin with a continuation byte.
    constexpr auto limit = note::search::SearchEngine::chunkBytesLimit;
    std::string text(limit - 513, 'x');
    text += "Î»"; // λ occupies [limit - 513, limit - 511)
    text.append(1000, 'x');
    const auto expected = text.size(); // inside the second window
    text += "needle";
    text.append(limit, 'x');
    note::editor::TextBuffer buffer(text);
    for (const bool regex : {false, true}) {
        note::search::SearchOptions options;
        options.regularExpression = regex;
        const auto result = note::search::SearchEngine{}.find(buffer, "needle", options);
        require(result.matches.size() == 1 && result.matches[0].begin == expected &&
                    result.matches[0].end == expected + 6,
                "matches after a chunk boundary inside a UTF-8 scalar keep exact byte offsets");
    }
    note::search::SearchOptions options;
    const auto lambda = note::search::SearchEngine{}.find(buffer, "Î»", options);
    require(lambda.matches.size() == 1 && lambda.matches[0].begin == limit - 513,
            "a scalar straddling a chunk boundary is found once at its byte offset");
}

int main() {
    testChunkBoundaryInsideScalar();
    note::editor::TextBuffer buffer("Alpha alpha BETA\nalpha");
    note::search::SearchOptions options;
    options.caseSensitive = true;
    options.wholeWord = true;
    auto result = note::search::SearchEngine{}.find(
        buffer, "alpha", options);
    require(result.status == note::search::SearchStatus::complete &&
            result.matches.size() == 2,
            "case-sensitive whole-word search finds two matches");

    options.caseSensitive = false;
    options.wholeWord = false;
    result = note::search::SearchEngine{}.find(buffer, "a", options);
    require(result.status == note::search::SearchStatus::complete &&
            !result.matches.empty(),
            "case-insensitive search finds matches");

    options.regularExpression = true;
    options.caseSensitive = true;
    result = note::search::SearchEngine{}.find(
        buffer, "alpha$", options);
    require(result.status == note::search::SearchStatus::complete &&
            result.matches.size() == 1,
            "regular-expression search executes");

    result = note::search::SearchEngine{}.find(
        buffer, "[", options);
    require(result.status == note::search::SearchStatus::invalidPattern,
            "invalid regular expression is reported");
    return 0;
}
