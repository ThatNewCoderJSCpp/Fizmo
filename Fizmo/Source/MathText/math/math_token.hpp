#ifndef FIZMO_MATHTEXT_MATH_TOKEN_HPP
#define FIZMO_MATHTEXT_MATH_TOKEN_HPP

#include <cstdint>
#include "../core/span.hpp"

namespace fizmo {
namespace mathtext {

#define FIZMO_MATHTEXT_MATH_TOKENS(X) \
    X(End) \
    X(Number) \
    X(BadNumber) \
    X(Identifier) \
    X(Command) \
    X(Glyph) \
    X(String) \
    X(BadString) \
    X(LeftParen) \
    X(RightParen) \
    X(LeftBrace) \
    X(RightBrace) \
    X(Comma) \
    X(Semicolon) \
    X(Plus) \
    X(Minus) \
    X(Star) \
    X(Slash) \
    X(Caret) \
    X(Underscore) \
    X(Bang) \
    X(Equal) \
    X(Less) \
    X(Greater) \
    X(LessEqual) \
    X(GreaterEqual) \
    X(NotEqual) \
    X(LoneBackslash) \
    X(UnclosedCommand) \
    X(Unknown)

enum class MathTokenKind : std::uint8_t {
#define FIZMO_MATHTEXT_X(name) name,
    FIZMO_MATHTEXT_MATH_TOKENS(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
};

inline constexpr const char* math_token_kind_name(MathTokenKind k) noexcept {
    switch (k) {
#define FIZMO_MATHTEXT_X(name) case MathTokenKind::name: return #name;
        FIZMO_MATHTEXT_MATH_TOKENS(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
    }
    return "Unknown";
}

struct MathToken {
    MathTokenKind kind = MathTokenKind::End;
    Span          span;
    std::uint32_t value = 0;

    constexpr bool is(MathTokenKind k) const noexcept { return kind == k; }
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_TOKEN_HPP
