#ifndef FIZMO_RANDOM_TEXT_HPP
#define FIZMO_RANDOM_TEXT_HPP

#include "random_std_int.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace fizmo {

namespace detail {

inline constexpr char random_alphanumeric[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
inline constexpr std::size_t random_alphanumeric_count = sizeof(random_alphanumeric) - 1;

inline void random_append_utf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    }
}

inline std::u32string random_decode_utf8(const std::string& s) {
    std::u32string out;
    std::size_t i = 0;

    while (i < s.size()) {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        std::size_t extra = 0;
        char32_t c = 0;

        if (b < 0x80) { c = b; }
        else if ((b & 0xE0) == 0xC0) { c = b & 0x1F; extra = 1; }
        else if ((b & 0xF0) == 0xE0) { c = b & 0x0F; extra = 2; }
        else if ((b & 0xF8) == 0xF0) { c = b & 0x07; extra = 3; }
        else throw std::invalid_argument("random text: the alphabet isn't valid UTF-8");

        for (std::size_t k = 1; k <= extra; ++k) {
            if (i + k >= s.size()) throw std::invalid_argument("random text: the alphabet isn't valid UTF-8");
            const unsigned char cont = static_cast<unsigned char>(s[i + k]);
            if ((cont & 0xC0) != 0x80) throw std::invalid_argument("random text: the alphabet isn't valid UTF-8");
            c = (c << 6) | (cont & 0x3F);
        }

        out += c;
        i += extra + 1;
    }

    return out;
}

inline bool random_valid_code_point(char32_t c) noexcept {
    return c <= 0x10FFFF && !(c >= 0xD800 && c <= 0xDFFF);
}

template <typename Below>
char32_t random_code_point(Below&& below, char32_t lo, char32_t hi) {
    if (lo > hi) { const char32_t t = lo; lo = hi; hi = t; }
    if (hi > 0x10FFFF) throw std::out_of_range("random_char: the range goes past the last Unicode code point (0x10FFFF)");
    const std::uint64_t total = static_cast<std::uint64_t>(hi) - lo + 1;
    const std::uint64_t gap_lo = lo > 0xD800 ? lo : 0xD800;
    const std::uint64_t gap_hi = hi < 0xDFFF ? hi : 0xDFFF;
    const std::uint64_t gap = gap_lo <= gap_hi ? gap_hi - gap_lo + 1 : 0;
    if (gap == total) throw std::out_of_range("random_char: the range only holds surrogate code points");
    std::uint64_t k = below(total - gap) + lo;
    if (gap != 0 && k >= gap_lo) k += gap;
    return static_cast<char32_t>(k);
}

template <typename Below>
char32_t random_alphabet_char(Below&& below, const std::u32string& alphabet) {
    if (alphabet.empty()) throw std::invalid_argument("random text: the alphabet is empty");
    return alphabet[static_cast<std::size_t>(below(alphabet.size()))];
}

template <typename Below>
std::string random_alphanumeric_string(Below&& below, std::size_t length) {
    std::string out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i) out += random_alphanumeric[static_cast<std::size_t>(below(random_alphanumeric_count))];
    return out;
}

template <typename Below>
std::string random_alphabet_string(Below&& below, std::size_t length, const std::string& alphabet) {
    const std::u32string chars = random_decode_utf8(alphabet);
    if (chars.empty()) throw std::invalid_argument("random text: the alphabet is empty");
    std::string out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i) random_append_utf8(out, chars[static_cast<std::size_t>(below(chars.size()))]);
    return out;
}

inline std::uint64_t secure_below(std::uint64_t n) {
    return random_int<std::uint64_t>(0, n - 1);
}

inline double secure_unit_double() {
    return static_cast<double>(random_int<std::uint64_t>() >> 11) * 0x1.0p-53;
}

} // namespace detail

inline bool random_bool(double probability) {
    if (!(probability > 0.0)) return false;
    if (probability >= 1.0) return true;
    return detail::secure_unit_double() < probability;
}

inline char32_t random_char() {
    return static_cast<char32_t>(detail::random_alphanumeric[static_cast<std::size_t>(detail::secure_below(detail::random_alphanumeric_count))]);
}

inline char32_t random_char(char32_t lo, char32_t hi) {
    return detail::random_code_point([](std::uint64_t n) { return detail::secure_below(n); }, lo, hi);
}

inline char32_t random_char(const std::string& alphabet) {
    return detail::random_alphabet_char([](std::uint64_t n) { return detail::secure_below(n); }, detail::random_decode_utf8(alphabet));
}

inline std::string random_string(std::size_t length) {
    return detail::random_alphanumeric_string([](std::uint64_t n) { return detail::secure_below(n); }, length);
}

inline std::string random_string(std::size_t length, const std::string& alphabet) {
    return detail::random_alphabet_string([](std::uint64_t n) { return detail::secure_below(n); }, length, alphabet);
}

} // namespace fizmo

#endif // FIZMO_RANDOM_TEXT_HPP
