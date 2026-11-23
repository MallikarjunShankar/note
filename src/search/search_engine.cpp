#include "search/search_engine.hpp"

#include <algorithm>
#include <chrono>

namespace note::search {

SearchResult SearchEngine::find(const editor::TextBuffer& buffer,
                                const std::string& query,
                                const SearchOptions& options,
                                std::size_t maxMatches) const {
    SearchResult result;
    if (query.empty()) return result;

    QRegularExpression regex;
    if (options.regularExpression) {
        if (query.size() > 512) {
            result.status = SearchStatus::aborted;
            return result;
        }
        regex = QRegularExpression(
            QString::fromUtf8(query.c_str(), static_cast<qsizetype>(query.size())),
            options.caseSensitive ? QRegularExpression::NoPatternOption
                                  : QRegularExpression::CaseInsensitiveOption);
        if (!regex.isValid()) {
            result.status = SearchStatus::invalidPattern;
            return result;
        }
    }

    const auto started = std::chrono::steady_clock::now();
    std::string pending;
    std::size_t windowBase = 0;
    const auto processWindow = [&] {
        QString chunk = QString::fromUtf8(pending.c_str(),
                                          static_cast<qsizetype>(pending.size()));
        const auto needle =
            QString::fromUtf8(query.c_str(),
                               static_cast<qsizetype>(query.size()));
        std::size_t localOffset = 0;
        while (localOffset < static_cast<std::size_t>(chunk.size()) &&
               result.matches.size() < maxMatches) {
            std::size_t begin = 0;
            std::size_t length = 0;
            if (options.regularExpression) {
                auto match = regex.match(chunk,
                                         static_cast<qsizetype>(localOffset));
                if (!match.hasMatch()) break;
                begin = static_cast<std::size_t>(match.capturedStart());
                length = static_cast<std::size_t>(match.capturedLength());
            } else {
                const auto found = chunk.indexOf(
                    needle, static_cast<qsizetype>(localOffset),
                    options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
                if (found < 0) break;
                begin = static_cast<std::size_t>(found);
                length = static_cast<std::size_t>(needle.size());
            }
            const auto end = begin + length;
            if (length == 0) {
                ++localOffset;
                continue;
            }
            if (options.wholeWord) {
                const auto isWord = [](QChar c) {
                    return c.isLetterOrNumber() || c == QLatin1Char('_');
                };
                if ((begin > 0 && isWord(chunk.at(static_cast<qsizetype>(begin - 1))) &&
                     isWord(chunk.at(static_cast<qsizetype>(begin)))) ||
                    (end < static_cast<std::size_t>(chunk.size()) &&
                     isWord(chunk.at(static_cast<qsizetype>(end))) &&
                     isWord(chunk.at(static_cast<qsizetype>(end - 1)))))
                {
                    localOffset = end;
                    continue;
                }
            }
            const auto byteBegin = windowBase +
                static_cast<std::size_t>(
                    chunk.left(static_cast<qsizetype>(begin)).toUtf8().size());
            const auto byteLength =
                static_cast<std::size_t>(
                    chunk.mid(static_cast<qsizetype>(begin),
                              static_cast<qsizetype>(length)).toUtf8().size());
            result.matches.push_back({byteBegin, byteBegin + byteLength});
            localOffset = end > begin ? end : begin + 1;
        }
    };

    (void)buffer.visitSegments(
        [&](std::string_view segment, std::span<const editor::LineEnding>) {
            pending.append(segment);
            while (pending.size() >= chunkBytesLimit) {
                // Cut and overlap only at scalar starts: a window beginning with a
                // continuation byte decodes to U+FFFD and shifts every byte offset.
                const auto scalarStart = [&](std::size_t index) {
                    while (index > 0 &&
                           (static_cast<unsigned char>(pending[index]) & 0xC0) == 0x80)
                        --index;
                    return index;
                };
                const auto cut = scalarStart(chunkBytesLimit);
                auto advance = scalarStart(cut > 512 ? cut - 512 : 0);
                if (advance == 0) advance = cut;
                std::string retained = pending.substr(advance);
                pending.resize(cut);
                processWindow();
                windowBase += advance;
                pending = std::move(retained);
                if (result.matches.size() >= maxMatches) return false;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - started).count() > 100) {
                    result.status = SearchStatus::aborted;
                    return false;
                }
            }
            return true;
        });

    if (result.status != SearchStatus::aborted && !pending.empty()) {
        processWindow();
    }
    std::sort(result.matches.begin(), result.matches.end(),
              [](const SearchMatch& left, const SearchMatch& right) {
                  return left.begin < right.begin;
              });
    result.matches.erase(
        std::unique(result.matches.begin(), result.matches.end(),
                    [](const SearchMatch& left, const SearchMatch& right) {
                        return left.begin == right.begin && left.end == right.end;
                    }),
        result.matches.end());
    if (result.status != SearchStatus::aborted &&
        result.matches.size() >= maxMatches)
        result.status = SearchStatus::aborted;
    return result;
}

} // namespace note::search
