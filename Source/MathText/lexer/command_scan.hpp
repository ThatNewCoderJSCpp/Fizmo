#ifndef FIZMO_MATHTEXT_COMMAND_SCAN_HPP
#define FIZMO_MATHTEXT_COMMAND_SCAN_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "chars.hpp"
#include "../core/utf8.hpp"

namespace fizmo {
namespace mathtext {

enum class CommandShape : std::uint8_t { Closed, Unclosed, Lone };

struct CommandScan {
    CommandShape shape = CommandShape::Lone;
    std::size_t  name_begin = 0;
    std::size_t  name_end = 0;
    std::size_t  end = 0;
};

constexpr bool is_command_name_char(char c) noexcept { return chars::is_letter(c) || chars::is_digit(c) || c == '_' || c == '-'; }

inline CommandScan scan_command(std::string_view s, std::size_t at, std::size_t limit) noexcept {
    CommandScan r;
    const std::size_t j = at + 1;
    r.name_begin = j;
    if (j >= limit || chars::is_space(s[j]) || s[j] == '\\') {
        r.shape = CommandShape::Lone;
        r.name_end = j;
        r.end = j;
        return r;
    }
    std::size_t k = j;
    if (is_command_name_char(s[j])) {
        while (k < limit && is_command_name_char(s[k])) ++k;
    } else {
        k = utf8::advance(s, j);
        if (k > limit) k = limit;
    }
    r.name_end = k;
    if (k < limit && s[k] == '\\') {
        r.shape = CommandShape::Closed;
        r.end = k + 1;
    } else {
        r.shape = CommandShape::Unclosed;
        r.end = k;
    }
    return r;
}

inline bool is_string_escape(std::string_view s, std::size_t at, std::size_t limit) noexcept {
    if (at + 1 >= limit) return false;
    const char c = s[at + 1];
    return chars::is_quote(c) || c == '\\';
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_COMMAND_SCAN_HPP
