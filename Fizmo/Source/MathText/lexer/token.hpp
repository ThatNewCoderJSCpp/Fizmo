#ifndef FIZMO_MATHTEXT_TOKEN_HPP
#define FIZMO_MATHTEXT_TOKEN_HPP

#include <cstdint>
#include "../core/span.hpp"

namespace fizmo {
namespace mathtext {

#define FIZMO_MATHTEXT_TOKENS(X) \
    X(End) \
    X(Identifier) \
    X(Command) \
    X(Number) \
    X(BadNumber) \
    X(String) \
    X(BadString) \
    X(LeftBrace) \
    X(RightBrace) \
    X(Comma) \
    X(LoneBackslash) \
    X(UnclosedCommand) \
    X(Unknown)

enum class TokenKind : std::uint8_t {
#define FIZMO_MATHTEXT_X(name) name,
    FIZMO_MATHTEXT_TOKENS(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
};

inline constexpr const char* token_kind_name(TokenKind k) noexcept {
    switch (k) {
#define FIZMO_MATHTEXT_X(name) case TokenKind::name: return #name;
        FIZMO_MATHTEXT_TOKENS(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
    }
    return "Unknown";
}

struct Token {
    TokenKind kind = TokenKind::End;
    Span      span;

    constexpr bool is(TokenKind k) const noexcept { return kind == k; }
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_TOKEN_HPP
