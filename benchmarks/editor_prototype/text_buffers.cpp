#include "text_buffers.hpp"

#include <algorithm>
#include <stdexcept>

namespace note::prototype {

GapBuffer::GapBuffer(std::size_t reserveBytes)
    : data_(reserveBytes + 4096), gapBegin_(0), gapEnd_(reserveBytes + 4096) {}

std::size_t GapBuffer::size() const noexcept {
    return data_.size() - (gapEnd_ - gapBegin_);
}

std::size_t PieceTable::allocatedBytes() const noexcept {
    return original_.capacity() + added_.capacity() + pieces_.capacity() * sizeof(Piece);
}

char GapBuffer::at(std::size_t position) const {
    if (position >= size()) throw std::out_of_range("gap-buffer position");
    const auto physical = position < gapBegin_ ? position : position + (gapEnd_ - gapBegin_);
    return data_[physical];
}

void GapBuffer::moveGap(std::size_t position) {
    if (position > size()) throw std::out_of_range("gap-buffer position");
    if (position < gapBegin_) {
        const auto count = gapBegin_ - position;
        std::move_backward(data_.begin() + static_cast<std::ptrdiff_t>(position),
                           data_.begin() + static_cast<std::ptrdiff_t>(gapBegin_),
                           data_.begin() + static_cast<std::ptrdiff_t>(gapEnd_));
        gapBegin_ -= count;
        gapEnd_ -= count;
    } else if (position > gapBegin_) {
        const auto count = position - gapBegin_;
        std::move(data_.begin() + static_cast<std::ptrdiff_t>(gapEnd_),
                  data_.begin() + static_cast<std::ptrdiff_t>(gapEnd_ + count),
                  data_.begin() + static_cast<std::ptrdiff_t>(gapBegin_));
        gapBegin_ += count;
        gapEnd_ += count;
    }
}

void GapBuffer::ensureGap(std::size_t required) {
    const auto gapSize = gapEnd_ - gapBegin_;
    if (gapSize >= required) return;
    const auto growth = std::max(required - gapSize, data_.size());
    std::vector<char> expanded(data_.size() + growth);
    const auto tailSize = data_.size() - gapEnd_;
    const auto newGapEnd = expanded.size() - tailSize;
    std::copy(data_.begin(), data_.begin() + static_cast<std::ptrdiff_t>(gapBegin_), expanded.begin());
    std::copy(data_.begin() + static_cast<std::ptrdiff_t>(gapEnd_), data_.end(),
              expanded.begin() + static_cast<std::ptrdiff_t>(newGapEnd));
    data_.swap(expanded);
    gapEnd_ = newGapEnd;
}

void GapBuffer::appendOriginal(std::string_view text) {
    insert(size(), text);
}

void GapBuffer::insert(std::size_t position, std::string_view text) {
    if (position > size()) throw std::out_of_range("gap-buffer insert");
    moveGap(position);
    ensureGap(text.size());
    std::copy(text.begin(), text.end(), data_.begin() + static_cast<std::ptrdiff_t>(gapBegin_));
    gapBegin_ += text.size();
}

void GapBuffer::erase(std::size_t position, std::size_t length) {
    if (position > size() || length > size() - position) throw std::out_of_range("gap-buffer erase");
    moveGap(position);
    gapEnd_ += length;
}

std::string GapBuffer::copy(std::size_t position, std::size_t length) const {
    if (position > size() || length > size() - position) throw std::out_of_range("gap-buffer copy");
    std::string result;
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i) result.push_back(at(position + i));
    return result;
}

PieceTable::PieceTable(std::size_t reserveBytes) {
    original_.reserve(reserveBytes);
    added_.reserve(std::min<std::size_t>(reserveBytes / 64, 4 * 1024 * 1024));
    pieces_.reserve(256);
}

std::string_view PieceTable::source(const Piece& piece) const noexcept {
    const auto& storage = piece.added ? added_ : original_;
    return std::string_view(storage).substr(piece.offset, piece.length);
}

char PieceTable::at(std::size_t position) const {
    if (position >= length_) throw std::out_of_range("piece-table position");
    std::size_t cursor = 0;
    for (const auto& piece : pieces_) {
        if (position < cursor + piece.length)
            return (piece.added ? added_ : original_)[piece.offset + position - cursor];
        cursor += piece.length;
    }
    throw std::out_of_range("piece-table position");
}

void PieceTable::appendOriginal(std::string_view text) {
    if (text.empty()) return;
    const auto offset = original_.size();
    original_.append(text);
    pieces_.push_back({false, offset, text.size()});
    length_ += text.size();
    normalize(pieces_);
}

void PieceTable::insert(std::size_t position, std::string_view text) {
    if (position > length_) throw std::out_of_range("piece-table insert");
    if (text.empty()) return;
    const auto addedOffset = added_.size();
    added_.append(text);
    const Piece inserted{true, addedOffset, text.size()};
    std::vector<Piece> next;
    next.reserve(pieces_.size() + 2);
    std::size_t cursor = 0;
    bool insertedPiece = false;
    for (const auto& piece : pieces_) {
        const auto end = cursor + piece.length;
        if (!insertedPiece && position <= end) {
            const auto before = position - cursor;
            if (before) next.push_back({piece.added, piece.offset, before});
            next.push_back(inserted);
            if (before < piece.length)
                next.push_back({piece.added, piece.offset + before, piece.length - before});
            insertedPiece = true;
        } else {
            next.push_back(piece);
        }
        cursor = end;
    }
    if (!insertedPiece) next.push_back(inserted);
    normalize(next);
    pieces_.swap(next);
    length_ += text.size();
}

void PieceTable::erase(std::size_t position, std::size_t length) {
    if (position > length_ || length > length_ - position) throw std::out_of_range("piece-table erase");
    if (!length) return;
    const auto eraseEnd = position + length;
    std::vector<Piece> next;
    next.reserve(pieces_.size() + 1);
    std::size_t cursor = 0;
    for (const auto& piece : pieces_) {
        const auto end = cursor + piece.length;
        if (end <= position || cursor >= eraseEnd) {
            next.push_back(piece);
        } else {
            if (cursor < position)
                next.push_back({piece.added, piece.offset, position - cursor});
            if (end > eraseEnd) {
                const auto skip = eraseEnd - cursor;
                next.push_back({piece.added, piece.offset + skip, end - eraseEnd});
            }
        }
        cursor = end;
    }
    normalize(next);
    pieces_.swap(next);
    length_ -= length;
}

std::string PieceTable::copy(std::size_t position, std::size_t length) const {
    if (position > length_ || length > length_ - position) throw std::out_of_range("piece-table copy");
    std::string result;
    result.reserve(length);
    const auto wantedEnd = position + length;
    std::size_t cursor = 0;
    for (const auto& piece : pieces_) {
        const auto end = cursor + piece.length;
        const auto overlapStart = std::max(cursor, position);
        const auto overlapEnd = std::min(end, wantedEnd);
        if (overlapStart < overlapEnd) {
            const auto sourceOffset = piece.offset + overlapStart - cursor;
            const auto count = overlapEnd - overlapStart;
            const auto& storage = piece.added ? added_ : original_;
            result.append(storage, sourceOffset, count);
        }
        if (end >= wantedEnd) break;
        cursor = end;
    }
    return result;
}

void PieceTable::normalize(std::vector<Piece>& pieces) const {
    std::vector<Piece> compact;
    compact.reserve(pieces.size());
    for (const auto& piece : pieces) {
        if (!piece.length) continue;
        if (!compact.empty() && compact.back().added == piece.added &&
            compact.back().offset + compact.back().length == piece.offset) {
            compact.back().length += piece.length;
        } else {
            compact.push_back(piece);
        }
    }
    pieces.swap(compact);
}

std::unique_ptr<TextBuffer> makeBuffer(std::string_view name, std::size_t reserveBytes) {
    if (name == "gap") return std::make_unique<GapBuffer>(reserveBytes);
    if (name == "piece") return std::make_unique<PieceTable>(reserveBytes);
    throw std::invalid_argument("buffer must be 'gap' or 'piece'");
}

} // namespace note::prototype
