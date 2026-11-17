#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace note::prototype {

class TextBuffer {
public:
    virtual ~TextBuffer() = default;
    virtual std::size_t size() const noexcept = 0;
    virtual std::size_t allocatedBytes() const noexcept = 0;
    virtual char at(std::size_t position) const = 0;
    virtual void appendOriginal(std::string_view text) = 0;
    virtual void insert(std::size_t position, std::string_view text) = 0;
    virtual void erase(std::size_t position, std::size_t length) = 0;
    virtual std::string copy(std::size_t position, std::size_t length) const = 0;
};

class GapBuffer final : public TextBuffer {
public:
    explicit GapBuffer(std::size_t reserveBytes = 0);
    std::size_t size() const noexcept override;
    std::size_t allocatedBytes() const noexcept override { return data_.capacity() * sizeof(char); }
    char at(std::size_t position) const override;
    void appendOriginal(std::string_view text) override;
    void insert(std::size_t position, std::string_view text) override;
    void erase(std::size_t position, std::size_t length) override;
    std::string copy(std::size_t position, std::size_t length) const override;

private:
    void moveGap(std::size_t position);
    void ensureGap(std::size_t required);
    std::vector<char> data_;
    std::size_t gapBegin_{};
    std::size_t gapEnd_{};
};

class PieceTable final : public TextBuffer {
public:
    explicit PieceTable(std::size_t reserveBytes = 0);
    std::size_t size() const noexcept override { return length_; }
    std::size_t allocatedBytes() const noexcept override;
    char at(std::size_t position) const override;
    void appendOriginal(std::string_view text) override;
    void insert(std::size_t position, std::string_view text) override;
    void erase(std::size_t position, std::size_t length) override;
    std::string copy(std::size_t position, std::size_t length) const override;
    std::size_t pieceCount() const noexcept { return pieces_.size(); }

private:
    struct Piece {
        bool added{};
        std::size_t offset{};
        std::size_t length{};
    };
    std::string_view source(const Piece& piece) const noexcept;
    void normalize(std::vector<Piece>& pieces) const;
    std::string original_;
    std::string added_;
    std::vector<Piece> pieces_;
    std::size_t length_{};
};

std::unique_ptr<TextBuffer> makeBuffer(std::string_view name, std::size_t reserveBytes);

} // namespace note::prototype
