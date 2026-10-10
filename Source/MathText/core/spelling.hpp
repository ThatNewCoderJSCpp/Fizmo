#ifndef FIZMO_MATHTEXT_SPELLING_HPP
#define FIZMO_MATHTEXT_SPELLING_HPP

#include <cstddef>
#include <string>
#include <string_view>

namespace fizmo {
namespace mathtext {

inline constexpr char normalize_spelling_char(char c) noexcept { return c == '-' ? '_' : c; }

inline constexpr bool is_spelling_separator(char c) noexcept { return c == '_' || c == '-'; }

inline constexpr bool spelling_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) if (normalize_spelling_char(a[i]) != normalize_spelling_char(b[i])) return false;
    return true;
}

inline constexpr int spelling_compare(std::string_view a, std::string_view b) noexcept {
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char x = static_cast<unsigned char>(normalize_spelling_char(a[i]));
        const unsigned char y = static_cast<unsigned char>(normalize_spelling_char(b[i]));
        if (x != y) return x < y ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

inline std::string normalized_spelling(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = normalize_spelling_char(c);
    return out;
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SPELLING_HPP
