#ifndef FIZMO_MATHTEXT_MATH_ALPHABET_HPP
#define FIZMO_MATHTEXT_MATH_ALPHABET_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include "../core/utf8.hpp"

namespace fizmo {
namespace mathtext {

inline std::uint32_t decode_utf8_at(std::string_view s, std::size_t i, std::size_t& next) noexcept {
    next = utf8::advance(s, i);
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const std::size_t n = next - i;
    if (n == 1) return c;
    if (n == 2) return ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
    if (n == 3) return ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
    return ((c & 0x07u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
}

inline std::uint32_t math_italic_codepoint(std::uint32_t cp) noexcept {
    if (cp == 'h') return 0x210E;
    if (cp >= 'a' && cp <= 'z') return 0x1D44E + (cp - 'a');
    if (cp >= 'A' && cp <= 'Z') return 0x1D434 + (cp - 'A');
    if (cp >= 0x3B1 && cp <= 0x3C9) return 0x1D6FC + (cp - 0x3B1);
    switch (cp) {
        case 0x3F5: return 0x1D716;
        case 0x3D1: return 0x1D717;
        case 0x3F0: return 0x1D718;
        case 0x3D5: return 0x1D719;
        case 0x3F1: return 0x1D71A;
        case 0x3D6: return 0x1D71B;
        case 0x2202: return 0x1D715;
        default: return cp;
    }
}

inline std::string to_math_italic(std::string_view text) {
    std::string out;
    out.reserve(text.size() * 4);
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i;
        const std::uint32_t cp = decode_utf8_at(text, i, next);
        utf8::append(out, math_italic_codepoint(cp));
        i = next;
    }
    return out;
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_ALPHABET_HPP
