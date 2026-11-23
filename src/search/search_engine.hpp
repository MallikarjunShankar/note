#pragma once

#include "editor/text_buffer.hpp"

#include <QRegularExpression>

#include <cstddef>
#include <string>
#include <vector>

namespace note::search {

enum class SearchStatus { complete, aborted, invalidPattern };

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
};

class SearchEngine final {
public:
    static constexpr std::size_t chunkBytesLimit{64 * 1024};
    [[nodiscard]] SearchResult find(const editor::TextBuffer& buffer,
                                    const std::string& query,
                                    const SearchOptions& options,
                                    std::size_t maxMatches = 10000) const;
};

} // namespace note::search
