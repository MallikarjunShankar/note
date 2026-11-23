#include "search/search_engine.hpp"

#include <chrono>
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

note::search::SearchResult findAll(const note::editor::TextBuffer& buffer,
                                   const std::string& query,
                                   const note::search::SearchOptions& options,
                                   std::chrono::milliseconds slice) {
    note::search::SearchResult all;
    for (std::size_t from = 0;;) {
        auto part = note::search::SearchEngine{}.find(
            buffer, query, options, 10000 - all.matches.size(), from, slice);
        require(part.status == note::search::SearchStatus::complete ||
                    part.resumeOffset > from,
                "every slice makes progress");
        all.matches.insert(all.matches.end(), part.matches.begin(), part.matches.end());
        all.status = part.status;
        from = part.resumeOffset;
        if (part.status != note::search::SearchStatus::incomplete) return all;
    }
}

void testLargeDocumentSearchReachesTheEnd() {
    // Regression: each window used to copy the rest of the document, so a
    // 12 MiB search hit the time budget and reported no matches.
    std::string text;
    const std::string line = "    const auto value = compute(index, buffer.size());\n";
    while (text.size() < 12u * 1024 * 1024) text += line;
    const auto expected = text.size();
    text += "needle\n";
    note::editor::TextBuffer buffer(text);
    for (const bool regex : {false, true}) {
        note::search::SearchOptions options;
        options.regularExpression = regex;
        const auto started = std::chrono::steady_clock::now();
        const auto result = note::search::SearchEngine{}.find(
            buffer, regex ? "need+le" : "needle", options, 10000, 0, std::chrono::seconds{30});
        require(result.status == note::search::SearchStatus::complete &&
                    result.matches.size() == 1 && result.matches[0].begin == expected,
                "a match at the end of a 12 MiB document is found");
        require(std::chrono::steady_clock::now() - started < std::chrono::seconds{10},
                "a 12 MiB search is linear, not quadratic");
        const auto sliced = findAll(buffer, regex ? "need+le" : "needle", options,
                                    std::chrono::milliseconds{0});
        require(sliced.matches.size() == 1 && sliced.matches[0].begin == expected,
                "sliced search resumes to the same single match");
    }
    note::search::SearchOptions options;
    const auto many = findAll(buffer, "value", options, std::chrono::milliseconds{0});
    require(many.status == note::search::SearchStatus::aborted && many.matches.size() == 10000,
            "match cap is reported as aborted");
    for (std::size_t i = 1; i < many.matches.size(); ++i)
        require(many.matches[i].begin > many.matches[i - 1].begin,
                "sliced matches are ordered and not duplicated across windows");
}

void testPathologicalRegexDoesNotBlock() {
    // Regression: catastrophic backtracking ran inside one window unchecked and
    // was then reported as a complete search with no matches.
    note::editor::TextBuffer buffer(std::string(256 * 1024, 'a'));
    note::search::SearchOptions options;
    options.regularExpression = true;
    const auto started = std::chrono::steady_clock::now();
    const auto result = note::search::SearchEngine{}.find(buffer, "(a+)+[bc]", options);
    require(std::chrono::steady_clock::now() - started < std::chrono::seconds{2},
            "a catastrophic regular expression stops within a bounded time");
    require(result.status == note::search::SearchStatus::aborted,
            "a catastrophic regular expression is reported as stopped, not complete");
}

void testLineAnchors() {
    // Windows are cut at line starts, so ^ and $ are line anchors everywhere.
    constexpr auto limit = note::search::SearchEngine::chunkBytesLimit;
    std::string text;
    std::size_t count = 0;
    while (text.size() < 2 * limit + 100) { // three windows, under the match cap
        text += "start middle end\n";
        ++count;
    }
    note::editor::TextBuffer buffer(text);
    note::search::SearchOptions options;
    options.regularExpression = true;
    options.caseSensitive = true;
    require(findAll(buffer, "^start", options, std::chrono::milliseconds{0}).matches.size() == count,
            "^ matches every line start, including at window boundaries");
    require(findAll(buffer, "end$", options, std::chrono::milliseconds{0}).matches.size() == count,
            "$ matches every line end");
    require(findAll(buffer, "^middle", options, std::chrono::milliseconds{0}).matches.empty(),
            "^ does not match mid-line");
}

int main() {
    testChunkBoundaryInsideScalar();
    testLargeDocumentSearchReachesTheEnd();
    testPathologicalRegexDoesNotBlock();
    testLineAnchors();
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
