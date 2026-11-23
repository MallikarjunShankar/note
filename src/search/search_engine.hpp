#pragma once

#include "editor/text_buffer.hpp"

#include <QRegularExpression>

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace note::search {

// incomplete: the time budget ran out; call again from resumeOffset.
// aborted: match cap reached, pattern over 512 bytes, or PCRE2 backtracking limit.
enum class SearchStatus { complete, incomplete, aborted, invalidPattern };

struct SearchOptions {
    bool caseSensitive{};
    bool wholeWord{};
    bool regularExpression{};
};

struct SearchMatch {
    std::size_t begin{};
    std::size_t end{};
};

struct SearchResult {
    SearchStatus status{SearchStatus::complete};
    std::vector<SearchMatch> matches;
    std::size_t resumeOffset{};
};

class SearchEngine final {
public:
    static constexpr std::size_t chunkBytesLimit{64 * 1024};
    // Searches windows of whole lines from `from` (a scalar boundary) until the
    // end, maxMatches, or `budget` is spent between windows.
    [[nodiscard]] SearchResult find(
        const editor::TextBuffer& buffer, const std::string& query,
        const SearchOptions& options, std::size_t maxMatches = 10000,
        std::size_t from = 0,
        std::chrono::milliseconds budget = std::chrono::milliseconds{100}) const;
};

} // namespace note::search
