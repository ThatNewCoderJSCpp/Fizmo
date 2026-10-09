#ifndef FIZMO_MATHTEXT_CHARS_HPP
#define FIZMO_MATHTEXT_CHARS_HPP

namespace fizmo {
namespace mathtext {
namespace chars {

constexpr bool is_space(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
constexpr bool is_letter(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
constexpr bool is_identifier_start(char c) noexcept { return is_letter(c) || c == '_'; }
constexpr bool is_identifier_char(char c) noexcept { return is_letter(c) || is_digit(c) || c == '_'; }
constexpr bool is_word_char(char c) noexcept { return is_identifier_char(c) || c == '.'; }
constexpr bool is_quote(char c) noexcept { return c == '"' || c == '\''; }
constexpr bool is_sign(char c) noexcept { return c == '+' || c == '-'; }

} // namespace chars
} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_CHARS_HPP
