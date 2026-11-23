#include "search/search_engine.hpp"

#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
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
