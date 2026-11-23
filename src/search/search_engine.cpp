#include "search/search_engine.hpp"

#include <algorithm>

namespace note::search {

SearchResult SearchEngine::find(const editor::TextBuffer& buffer,
                                const std::string& query,
                                const SearchOptions& options,
                                std::size_t maxMatches,
                                std::size_t from,
                                std::chrono::milliseconds budget) const {
    SearchResult result;
    result.resumeOffset = from;
    if (query.empty()) return result;

    const auto needle =
        QString::fromUtf8(query.c_str(), static_cast<qsizetype>(query.size()));
    QRegularExpression regex;
    if (options.regularExpression) {
        if (query.size() > 512) {
            result.status = SearchStatus::aborted;
            return result;
        }
        // Windows start at line starts, so ^ and $ mean line start/end.
        QRegularExpression::PatternOptions patternOptions =
            QRegularExpression::MultilineOption;
        if (!options.caseSensitive)
            patternOptions |= QRegularExpression::CaseInsensitiveOption;
        regex = QRegularExpression(needle, patternOptions);
        if (!regex.isValid()) {
            result.status = SearchStatus::invalidPattern;
            return result;
        }
    }

    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    const auto isWord = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_');
    };

    // Searches one window; matches starting at or after ownedBytes belong to the
    // next window. Returns false if the search must stop.
    const auto processWindow = [&](const std::string& text, std::size_t base,
                                   std::size_t ownedBytes) {
        const auto chunk =
            QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
        // Matches arrive in order, so UTF-16 -> UTF-8 offsets are traced forward.
        qsizetype tracedChar = 0;
        std::size_t tracedByte = 0;
        const auto byteAt = [&](qsizetype index) {
            for (; tracedChar < index; ++tracedChar) {
                const auto c = chunk.at(tracedChar).unicode();
                tracedByte += c < 0x80 ? 1 : c < 0x800 ? 2 : QChar::isSurrogate(c) ? 2 : 3;
            }
            return tracedByte;
        };
        qsizetype offset = 0;
        while (offset < chunk.size() && result.matches.size() < maxMatches) {
            qsizetype begin = 0;
            qsizetype length = 0;
            if (options.regularExpression) {
                const auto match = regex.match(chunk, offset);
                // Invalid here means PCRE2 hit its backtracking limit; the default
                // limit bounds one call (~50-80 ms measured on a 4 GB i5 laptop).
                if (!match.isValid()) {
                    result.status = SearchStatus::aborted;
                    return false;
                }
                if (!match.hasMatch()) break;
                begin = match.capturedStart();
                length = match.capturedLength();
            } else {
                begin = chunk.indexOf(needle, offset,
                                      options.caseSensitive ? Qt::CaseSensitive
                                                            : Qt::CaseInsensitive);
                if (begin < 0) break;
                length = needle.size();
            }
            if (length == 0) {
                offset = begin + 1;
                if (offset < chunk.size() && chunk.at(offset).isLowSurrogate()) ++offset;
                continue;
            }
            const auto end = begin + length;
            if (options.wholeWord &&
                ((begin > 0 && isWord(chunk.at(begin - 1)) && isWord(chunk.at(begin))) ||
                 (end < chunk.size() && isWord(chunk.at(end)) && isWord(chunk.at(end - 1))))) {
                offset = end;
                continue;
            }
            const auto byteBegin = byteAt(begin);
            if (byteBegin >= ownedBytes) break;
            result.matches.push_back({base + byteBegin, base + byteAt(end)});
            offset = end;
        }
        return true;
    };

    const auto size = buffer.sizeBytes();
    auto position = std::min(from, size);
    while (position < size) {
        auto text = buffer.textRange(position, std::min(chunkBytesLimit, size - position));
        auto owned = text.size();
        if (position + text.size() < size) {
            // Cut after the last line break so the next window starts a line. A
            // longer line is cut at a scalar start, overlapping by 512 bytes.
            const auto newline = text.rfind('\n');
            if (newline != std::string::npos) {
                text.resize(newline + 1);
                owned = text.size();
            } else {
                auto cut = text.size();
                while (cut > 0 && !buffer.isScalarBoundary(position + cut)) --cut;
                text.resize(cut);
                owned = cut > 512 ? cut - 512 : cut;
                while (owned > 0 && !buffer.isScalarBoundary(position + owned)) --owned;
                if (owned == 0) owned = cut;
            }
        }
        if (!processWindow(text, position, owned)) return result;
        position += owned;
        result.resumeOffset = position;
        if (result.matches.size() >= maxMatches) {
            result.status = SearchStatus::aborted;
            return result;
        }
        if (position < size && Clock::now() - started > budget) {
            result.status = SearchStatus::incomplete;
            return result;
        }
    }
    return result;
}

} // namespace note::search
