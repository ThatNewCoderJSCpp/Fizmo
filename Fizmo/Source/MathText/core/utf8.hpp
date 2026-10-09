#ifndef FIZMO_MATHTEXT_UTF8_HPP
#define FIZMO_MATHTEXT_UTF8_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace fizmo {
namespace mathtext {
namespace utf8 {

constexpr bool is_continuation(unsigned char c) noexcept { return (c & 0xC0) == 0x80; }

constexpr std::size_t sequence_length(unsigned char lead) noexcept {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

inline std::size_t advance(std::string_view s, std::size_t i) noexcept {
    if (i >= s.size()) return s.size();
    std::size_t n = sequence_length(static_cast<unsigned char>(s[i]));
    std::size_t j = i + 1;
    while (j < s.size() && j < i + n && is_continuation(static_cast<unsigned char>(s[j]))) ++j;
    return j;
}

inline std::size_t count_codepoints(std::string_view s) noexcept {
    std::size_t n = 0;
    for (unsigned char c : s) if (!is_continuation(c)) ++n;
    return n;
}

inline void append(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

inline std::string encode(char32_t cp) {
    std::string s;
    append(s, cp);
    return s;
}

} // namespace utf8
} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_UTF8_HPP
