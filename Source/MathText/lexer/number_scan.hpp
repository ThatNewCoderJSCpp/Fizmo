#ifndef FIZMO_MATHTEXT_NUMBER_SCAN_HPP
#define FIZMO_MATHTEXT_NUMBER_SCAN_HPP

#include <cstddef>
#include <string_view>
#include "chars.hpp"

namespace fizmo {
namespace mathtext {

struct NumberShape {
    bool        valid = false;
    std::size_t mantissa_end = 0;
    std::size_t exponent_begin = 0;
    char        marker = 0;
};

inline NumberShape analyze_number(std::string_view text) noexcept {
    NumberShape r;
    std::size_t i = 0;
    const std::size_t n = text.size();
    if (i < n && chars::is_sign(text[i])) ++i;
    std::size_t digits = 0;
    while (i < n && chars::is_digit(text[i])) { ++i; ++digits; }
    if (i < n && text[i] == '.') {
        ++i;
        while (i < n && chars::is_digit(text[i])) { ++i; ++digits; }
    }
    if (digits == 0) return r;
    r.mantissa_end = i;
    if (i < n && (text[i] == 'e' || text[i] == 'E')) {
        r.marker = text[i];
        ++i;
        r.exponent_begin = i;
        if (i < n && chars::is_sign(text[i])) ++i;
        std::size_t exp_digits = 0;
        while (i < n && chars::is_digit(text[i])) { ++i; ++exp_digits; }
        if (exp_digits == 0) return r;
    } else {
        r.exponent_begin = i;
    }
    r.valid = i == n;
    return r;
}

inline bool starts_number(std::string_view s, std::size_t i) noexcept {
    if (i >= s.size()) return false;
    if (chars::is_sign(s[i])) ++i;
    if (i >= s.size()) return false;
    if (chars::is_digit(s[i])) return true;
    return s[i] == '.' && i + 1 < s.size() && chars::is_digit(s[i + 1]);
}

inline std::size_t scan_number_run(std::string_view s, std::size_t i) noexcept {
    const std::size_t n = s.size();
    if (i < n && chars::is_sign(s[i])) ++i;
    while (i < n) {
        const char c = s[i];
        if (chars::is_word_char(c)) { ++i; continue; }
        if (chars::is_sign(c) && (s[i - 1] == 'e' || s[i - 1] == 'E') && i + 1 < n && chars::is_digit(s[i + 1])) { ++i; continue; }
        break;
    }
    return i;
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_NUMBER_SCAN_HPP
