#include "editor/text_buffer.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace note::editor {
namespace {

struct NormalizedText {
    std::string text;
    std::vector<LineEnding> endings;
};

bool validUtf8(std::string_view value) noexcept {
    std::size_t i = 0;
    while (i < value.size()) {
        const auto c = static_cast<unsigned char>(value[i]);
        if (c <= 0x7f) { ++i; continue; }
        std::size_t count = 0;
        std::uint32_t codepoint = 0;
        if (c >= 0xc2 && c <= 0xdf) { count = 2; codepoint = c & 0x1f; }
        else if (c >= 0xe0 && c <= 0xef) { count = 3; codepoint = c & 0x0f; }
        else if (c >= 0xf0 && c <= 0xf4) { count = 4; codepoint = c & 0x07; }
        else return false;
        if (i + count > value.size()) return false;
        for (std::size_t j = 1; j < count; ++j) {
            const auto next = static_cast<unsigned char>(value[i + j]);
            if ((next & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if ((count == 3 && codepoint < 0x800) || (count == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) return false;
        i += count;
    }
    return true;
}

NormalizedText normalize(std::string_view input, LineEnding bareLineEnding = LineEnding::lf) {
    if (!validUtf8(input)) throw std::invalid_argument("text buffer input must be valid UTF-8");
    NormalizedText result;
    result.text.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r') {
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                result.endings.push_back(LineEnding::crlf);
                ++i;
            } else {
                result.endings.push_back(LineEnding::cr);
            }
            result.text.push_back('\n');
        } else if (input[i] == '\n') {
            result.endings.push_back(bareLineEnding);
            result.text.push_back('\n');
        } else {
            result.text.push_back(input[i]);
        }
    }
    return result;
}

} // namespace

struct TextBuffer::Impl {
    struct Source {
        std::string text;
        std::vector<std::size_t> newlineOffsets;
        std::vector<LineEnding> endings;

        void append(std::string_view value, const std::vector<LineEnding>& valueEndings) {
            const auto base = text.size();
            text.append(value);
            std::size_t endingIndex = 0;
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '\n') {
                    newlineOffsets.push_back(base + i);
                    endings.push_back(valueEndings.at(endingIndex++));
                }
            }
            if (endingIndex != valueEndings.size()) throw std::logic_error("newline metadata mismatch");
        }
    };

    struct Piece {
        bool added{};
        std::size_t offset{};
        std::size_t length{};
    };

    struct Node {
        explicit Node(Piece value, std::uint64_t priorityValue)
            : piece(value), priority(priorityValue) {}
        Piece piece;
        std::uint64_t priority{};
        std::size_t subtreeBytes{};
        std::size_t subtreeNewlines{};
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
    };

    struct Split { std::unique_ptr<Node> left; std::unique_ptr<Node> right; };

    Source original;
    Source added;
    std::unique_ptr<Node> root;
    std::uint64_t currentRevision{};
    std::uint64_t randomState{0x4e4f54455f563100ULL};

    [[nodiscard]] Source& source(bool isAdded) noexcept { return isAdded ? added : original; }
    [[nodiscard]] const Source& source(bool isAdded) const noexcept { return isAdded ? added : original; }
    [[nodiscard]] std::size_t bytes(const std::unique_ptr<Node>& node) const noexcept {
        return node ? node->subtreeBytes : 0;
    }
    [[nodiscard]] std::size_t newlines(const std::unique_ptr<Node>& node) const noexcept {
        return node ? node->subtreeNewlines : 0;
    }
    [[nodiscard]] std::size_t pieceNewlines(const Piece& piece) const {
        const auto& offsets = source(piece.added).newlineOffsets;
        const auto first = std::lower_bound(offsets.begin(), offsets.end(), piece.offset);
        const auto last = std::lower_bound(first, offsets.end(), piece.offset + piece.length);
        return static_cast<std::size_t>(last - first);
    }
    std::uint64_t nextPriority() noexcept {
        randomState += 0x9e3779b97f4a7c15ULL;
        auto value = randomState;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }
    std::unique_ptr<Node> makeNode(Piece piece) {
        auto node = std::make_unique<Node>(piece, nextPriority());
        update(*node);
        return node;
    }
    void update(Node& node) {
        node.subtreeBytes = bytes(node.left) + node.piece.length + bytes(node.right);
        node.subtreeNewlines = newlines(node.left) + pieceNewlines(node.piece) + newlines(node.right);
    }
    std::unique_ptr<Node> merge(std::unique_ptr<Node> left, std::unique_ptr<Node> right) {
        if (!left) return right;
        if (!right) return left;
        if (left->priority < right->priority) {
            left->right = merge(std::move(left->right), std::move(right));
            update(*left);
            return left;
        }
        right->left = merge(std::move(left), std::move(right->left));
        update(*right);
        return right;
    }
    std::pair<std::unique_ptr<Node>, std::unique_ptr<Node>> popRightmost(std::unique_ptr<Node> node) {
        if (node->right) {
            auto parts = popRightmost(std::move(node->right));
            node->right = std::move(parts.first);
            update(*node);
            return {std::move(node), std::move(parts.second)};
        }
        auto rest = std::move(node->left);
        update(*node);
        return {std::move(rest), std::move(node)};
    }
    std::pair<std::unique_ptr<Node>, std::unique_ptr<Node>> popLeftmost(std::unique_ptr<Node> node) {
        if (node->left) {
            auto parts = popLeftmost(std::move(node->left));
            node->left = std::move(parts.second);
            update(*node);
            return {std::move(parts.first), std::move(node)};
        }
        auto rest = std::move(node->right);
        update(*node);
        return {std::move(node), std::move(rest)};
    }
    bool contiguous(const Node& left, const Node& right) const noexcept {
        return left.piece.added == right.piece.added &&
            left.piece.offset + left.piece.length == right.piece.offset;
    }
    std::unique_ptr<Node> mergeCoalescing(std::unique_ptr<Node> left, std::unique_ptr<Node> right) {
        if (!left) return right;
        if (!right) return left;
        auto leftParts = popRightmost(std::move(left));
        auto rightParts = popLeftmost(std::move(right));
        if (contiguous(*leftParts.second, *rightParts.first)) {
            leftParts.second->piece.length += rightParts.first->piece.length;
            leftParts.second->left.reset();
            leftParts.second->right.reset();
            update(*leftParts.second);
            return merge(merge(std::move(leftParts.first), std::move(leftParts.second)),
                         std::move(rightParts.second));
        }
        auto restoredLeft = merge(std::move(leftParts.first), std::move(leftParts.second));
        auto restoredRight = merge(std::move(rightParts.first), std::move(rightParts.second));
        return merge(std::move(restoredLeft), std::move(restoredRight));
    }
    Split split(std::unique_ptr<Node> node, std::size_t position) {
        if (!node) return {};
        const auto leftBytes = bytes(node->left);
        const auto pieceEnd = leftBytes + node->piece.length;
        if (position < leftBytes) {
            auto parts = split(std::move(node->left), position);
            node->left = std::move(parts.right);
            update(*node);
            return {std::move(parts.left), std::move(node)};
        }
        if (position > pieceEnd) {
            auto parts = split(std::move(node->right), position - pieceEnd);
            node->right = std::move(parts.left);
            update(*node);
            return {std::move(node), std::move(parts.right)};
        }
        if (position == leftBytes) {
            auto before = std::move(node->left);
            update(*node);
            return {std::move(before), std::move(node)};
        }
        if (position == pieceEnd) {
            auto after = std::move(node->right);
            update(*node);
            return {std::move(node), std::move(after)};
        }

        const auto leftLength = position - leftBytes;
        const auto oldPiece = node->piece;
        auto beforeTree = std::move(node->left);
        auto afterTree = std::move(node->right);
        node.reset();
        auto beforePiece = makeNode({oldPiece.added, oldPiece.offset, leftLength});
        auto afterPiece = makeNode({oldPiece.added, oldPiece.offset + leftLength,
                                    oldPiece.length - leftLength});
        return {merge(std::move(beforeTree), std::move(beforePiece)),
                merge(std::move(afterPiece), std::move(afterTree))};
    }
    bool boundary(std::size_t position) const {
        if (position > bytes(root)) return false;
        if (position == 0 || position == bytes(root)) return true;
        const auto next = byteAt(position);
        return (static_cast<unsigned char>(next) & 0xc0) != 0x80;
    }
    char byteAt(std::size_t position) const {
        auto* node = root.get();
        while (node) {
            const auto leftBytes = node->left ? node->left->subtreeBytes : 0;
            if (position < leftBytes) { node = node->left.get(); continue; }
            position -= leftBytes;
            if (position < node->piece.length)
                return source(node->piece.added).text[node->piece.offset + position];
            position -= node->piece.length;
            node = node->right.get();
        }
        throw std::out_of_range("text buffer position");
    }
    std::size_t findNewline(std::size_t index) const {
        auto* node = root.get();
        std::size_t base = 0;
        while (node) {
            const auto leftLines = node->left ? node->left->subtreeNewlines : 0;
            const auto leftSize = node->left ? node->left->subtreeBytes : 0;
            if (index < leftLines) { node = node->left.get(); continue; }
            index -= leftLines;
            const auto& offsets = source(node->piece.added).newlineOffsets;
            const auto first = std::lower_bound(offsets.begin(), offsets.end(), node->piece.offset);
            const auto last = std::lower_bound(first, offsets.end(), node->piece.offset + node->piece.length);
            const auto pieceLines = static_cast<std::size_t>(last - first);
            if (index < pieceLines)
                return base + leftSize + (*(first + static_cast<std::ptrdiff_t>(index)) - node->piece.offset);
            index -= pieceLines;
            base += leftSize + node->piece.length;
            node = node->right.get();
        }
        throw std::out_of_range("newline index");
    }
    LineEnding findEnding(std::size_t index) const {
        auto* node = root.get();
        while (node) {
            const auto leftLines = node->left ? node->left->subtreeNewlines : 0;
            if (index < leftLines) { node = node->left.get(); continue; }
            index -= leftLines;
            const auto& src = source(node->piece.added);
            const auto first = std::lower_bound(src.newlineOffsets.begin(), src.newlineOffsets.end(), node->piece.offset);
            const auto last = std::lower_bound(first, src.newlineOffsets.end(), node->piece.offset + node->piece.length);
            const auto pieceLines = static_cast<std::size_t>(last - first);
            if (index < pieceLines) {
                const auto sourceIndex = static_cast<std::size_t>(first - src.newlineOffsets.begin()) + index;
                return src.endings[sourceIndex];
            }
            index -= pieceLines;
            node = node->right.get();
        }
        throw std::out_of_range("newline index");
    }
    void appendRange(const Node* node, std::size_t begin, std::size_t end, std::string& output) const {
        if (!node || begin >= end) return;
        const auto leftSize = node->left ? node->left->subtreeBytes : 0;
        if (begin < leftSize)
            appendRange(node->left.get(), begin, std::min(end, leftSize), output);
        const auto pieceBegin = leftSize;
        const auto pieceEnd = pieceBegin + node->piece.length;
        const auto overlapBegin = std::max(begin, pieceBegin);
        const auto overlapEnd = std::min(end, pieceEnd);
        if (overlapBegin < overlapEnd) {
            const auto offset = node->piece.offset + overlapBegin - pieceBegin;
            output.append(source(node->piece.added).text, offset, overlapEnd - overlapBegin);
        }
        if (end > pieceEnd)
            appendRange(node->right.get(), begin > pieceEnd ? begin - pieceEnd : 0,
                        end - pieceEnd, output);
    }
    void appendSnapshot(const Node* node, TextSnapshot& output) const {
        if (!node) return;
        appendSnapshot(node->left.get(), output);
        const auto& src = source(node->piece.added);
        output.utf8.append(src.text, node->piece.offset, node->piece.length);
        const auto first = std::lower_bound(src.newlineOffsets.begin(), src.newlineOffsets.end(), node->piece.offset);
        const auto last = std::lower_bound(first, src.newlineOffsets.end(), node->piece.offset + node->piece.length);
        const auto firstIndex = static_cast<std::size_t>(first - src.newlineOffsets.begin());
        const auto lastIndex = static_cast<std::size_t>(last - src.newlineOffsets.begin());
        output.lineEndings.insert(output.lineEndings.end(),
                                  src.endings.begin() + static_cast<std::ptrdiff_t>(firstIndex),
                                  src.endings.begin() + static_cast<std::ptrdiff_t>(lastIndex));
        appendSnapshot(node->right.get(), output);
    }
    std::size_t countNodes(const Node* node) const noexcept {
        return node ? 1 + countNodes(node->left.get()) + countNodes(node->right.get()) : 0;
    }
    std::unique_ptr<Node> appendToAdded(const NormalizedText& value) {
        if (value.text.empty()) return {};
        const auto offset = added.text.size();
        added.append(value.text, value.endings);
        return makeNode({true, offset, value.text.size()});
    }
};

TextBuffer::TextBuffer() : impl_(std::make_unique<Impl>()) {}
TextBuffer::TextBuffer(std::string_view utf8Text) : TextBuffer() {
    auto value = normalize(utf8Text);
    if (!value.text.empty()) {
        impl_->original.append(value.text, value.endings);
        impl_->root = impl_->makeNode({false, 0, value.text.size()});
    }
}
TextBuffer::TextBuffer(TextSnapshot canonicalText) : TextBuffer() {
    if (!validUtf8(canonicalText.utf8) || canonicalText.utf8.find('\r') != std::string::npos ||
        static_cast<std::size_t>(std::count(canonicalText.utf8.begin(), canonicalText.utf8.end(), '\n')) !=
            canonicalText.lineEndings.size())
        throw std::invalid_argument("canonical text snapshot is inconsistent");
    if (!canonicalText.utf8.empty()) {
        impl_->original.append(canonicalText.utf8, canonicalText.lineEndings);
        impl_->root = impl_->makeNode({false, 0, canonicalText.utf8.size()});
    }
}
TextBuffer::~TextBuffer() = default;
TextBuffer::TextBuffer(TextBuffer&&) noexcept = default;
TextBuffer& TextBuffer::operator=(TextBuffer&&) noexcept = default;

std::size_t TextBuffer::sizeBytes() const noexcept { return impl_->bytes(impl_->root); }
std::size_t TextBuffer::lineCount() const noexcept { return impl_->newlines(impl_->root) + 1; }
std::uint64_t TextBuffer::revision() const noexcept { return impl_->currentRevision; }
std::size_t TextBuffer::pieceCount() const noexcept { return impl_->countNodes(impl_->root.get()); }
std::size_t TextBuffer::estimatedStorageBytes() const noexcept {
    const auto& original = impl_->original;
    const auto& added = impl_->added;
    return original.text.capacity() + added.text.capacity() +
        (original.newlineOffsets.capacity() + added.newlineOffsets.capacity()) * sizeof(std::size_t) +
        (original.endings.capacity() + added.endings.capacity()) * sizeof(LineEnding) +
        pieceCount() * sizeof(Impl::Node);
}
std::string TextBuffer::text() const { return textRange(0, sizeBytes()); }
std::string TextBuffer::textRange(std::size_t offset, std::size_t length) const {
    if (offset > sizeBytes() || length > sizeBytes() - offset) throw std::out_of_range("text range");
    if (!impl_->boundary(offset) || !impl_->boundary(offset + length))
        throw std::invalid_argument("text range must start and end on UTF-8 boundaries");
    std::string output;
    output.reserve(length);
    impl_->appendRange(impl_->root.get(), offset, offset + length, output);
    return output;
}
LineRange TextBuffer::lineRange(std::size_t line) const {
    if (line >= lineCount()) throw std::out_of_range("line index");
    const auto newlineCount = impl_->newlines(impl_->root);
    const auto begin = line == 0 ? 0 : impl_->findNewline(line - 1) + 1;
    const auto end = line < newlineCount ? impl_->findNewline(line) : sizeBytes();
    return {begin, end};
}
std::string TextBuffer::lineText(std::size_t line) const {
    const auto range = lineRange(line);
    return textRange(range.begin, range.end - range.begin);
}
bool TextBuffer::isScalarBoundary(std::size_t offset) const {
    return offset <= sizeBytes() && impl_->boundary(offset);
}
unsigned char TextBuffer::byteAt(std::size_t offset) const {
    if (offset >= sizeBytes()) throw std::out_of_range("text buffer byte index");
    return static_cast<unsigned char>(impl_->byteAt(offset));
}
LineEnding TextBuffer::lineEnding(std::size_t newlineIndex) const {
    if (newlineIndex >= impl_->newlines(impl_->root)) throw std::out_of_range("newline index");
    return impl_->findEnding(newlineIndex);
}
TextSnapshot TextBuffer::snapshot() const {
    TextSnapshot result;
    result.utf8.reserve(sizeBytes());
    result.lineEndings.reserve(impl_->newlines(impl_->root));
    impl_->appendSnapshot(impl_->root.get(), result);
    return result;
}
void TextBuffer::insert(std::size_t offset, std::string_view utf8Text, LineEnding bareLineEnding) {
    replace(offset, 0, utf8Text, bareLineEnding);
}
void TextBuffer::erase(std::size_t offset, std::size_t length) { replace(offset, length, {}); }
void TextBuffer::replace(std::size_t offset, std::size_t length, std::string_view utf8Text,
                         LineEnding bareLineEnding) {
    if (offset > sizeBytes() || length > sizeBytes() - offset) throw std::out_of_range("replace range");
    if (!impl_->boundary(offset) || !impl_->boundary(offset + length))
        throw std::invalid_argument("edit range must start and end on UTF-8 boundaries");
    auto value = normalize(utf8Text, bareLineEnding);
    if (length == 0 && value.text.empty()) return;
    auto first = impl_->split(std::move(impl_->root), offset);
    auto second = impl_->split(std::move(first.right), length);
    auto addedPiece = impl_->appendToAdded(value);
    auto leftWithInsertion = impl_->mergeCoalescing(std::move(first.left), std::move(addedPiece));
    impl_->root = impl_->mergeCoalescing(std::move(leftWithInsertion), std::move(second.right));
    if (length != 0 || !value.text.empty()) ++impl_->currentRevision;
}

} // namespace note::editor
