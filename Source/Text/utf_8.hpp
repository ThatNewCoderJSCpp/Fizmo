#ifndef FIZMO_TEXT_UTF8_HPP
#define FIZMO_TEXT_UTF8_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <iterator>

namespace fizmo {
namespace text {
namespace utf8 {

// Replacement codepoint for malformed sequences
static constexpr std::uint32_t kReplacement = 0xFFFD;

inline std::uint32_t decode_one(const char*& pos, const char* end) noexcept {
    if (pos >= end) return kReplacement;
    auto b0 = static_cast<std::uint8_t>(*pos);

    if (b0 < 0x80) {
        ++pos;
        return b0;
    }

    std::uint32_t cp;
    int remaining;

    if ((b0 & 0xE0) == 0xC0)      { cp = b0 & 0x1F; remaining = 1; }
    else if ((b0 & 0xF0) == 0xE0) { cp = b0 & 0x0F; remaining = 2; }
    else if ((b0 & 0xF8) == 0xF0) { cp = b0 & 0x07; remaining = 3; }
    else { ++pos; return kReplacement; }   // invalid lead byte

    if (pos + 1 + remaining > end) { ++pos; return kReplacement; }
    ++pos;

    for (int i = 0; i < remaining; ++i) {
        auto cb = static_cast<std::uint8_t>(*pos);
        if ((cb & 0xC0) != 0x80) return kReplacement;  
        cp = (cp << 6) | (cb & 0x3F);
        ++pos;
    }

    if (remaining == 1 && cp < 0x80)    return kReplacement;
    if (remaining == 2 && cp < 0x800)   return kReplacement;
    if (remaining == 3 && cp < 0x10000) return kReplacement;
    if (cp >= 0xD800 && cp <= 0xDFFF)   return kReplacement;
    if (cp > 0x10FFFF)                  return kReplacement;
    return cp;
}

class CodepointIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type        = std::uint32_t;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const std::uint32_t*;
    using reference         = std::uint32_t;

    CodepointIterator() noexcept : m_pos(nullptr), m_end(nullptr), m_cp(0) {}

    CodepointIterator(const char* pos, const char* end) noexcept : m_pos(pos), m_end(end), m_cp(0) {
        if (m_pos < m_end) m_cp = decode_one(m_pos, m_end);
    }

    std::uint32_t operator*()  const noexcept { return m_cp; }

    CodepointIterator& operator++() noexcept {
        if (m_pos < m_end) m_cp = decode_one(m_pos, m_end);
        else               m_pos = m_end + 1; // sentinel
        return *this;
    }

    CodepointIterator operator++(int) noexcept { auto tmp = *this; ++(*this); return tmp; }

    bool operator==(const CodepointIterator& o) const noexcept {
        // Both exhausted?
        bool a_done = (m_pos > m_end);
        bool b_done = (o.m_pos > o.m_end);
        if (a_done && b_done) return true;
        if (a_done != b_done) return false;
        return m_pos == o.m_pos;
    }

    bool operator!=(const CodepointIterator& o) const noexcept { return !(*this == o); }

private:
    const char*   m_pos;
    const char*   m_end;
    std::uint32_t m_cp;
};

class CodepointRange {
public:
    CodepointRange(const char* data, std::size_t len) noexcept : m_data(data), m_len(len) {}
    explicit CodepointRange(const std::string& s) noexcept : m_data(s.data()), m_len(s.size()) {}
    CodepointIterator begin() const noexcept { return { m_data, m_data + m_len }; }

    CodepointIterator end()   const noexcept {
        const char* e = m_data + m_len;
        return { e + 1, e };  // exhausted sentinel
    }

private:
    const char*  m_data;
    std::size_t  m_len;
};

inline CodepointRange codepoints(const std::string& s) noexcept {
    return CodepointRange(s);
}

inline CodepointRange codepoints(const char* s, std::size_t len) noexcept {
    return CodepointRange(s, len);
}

inline std::size_t count_codepoints(const std::string& s) noexcept {
    std::size_t n = 0;
    const char* p = s.data();
    const char* e = p + s.size();
    while (p < e) { decode_one(p, e); ++n; }
    return n;
}

} // namespace utf8
} // namespace text
} // namespace fizmo

#endif // FIZMO_TEXT_UTF8_HPP