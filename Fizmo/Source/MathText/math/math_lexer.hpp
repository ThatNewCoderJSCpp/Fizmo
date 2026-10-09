#ifndef FIZMO_MATHTEXT_MATH_LEXER_HPP
#define FIZMO_MATHTEXT_MATH_LEXER_HPP

#include <cstddef>
#include <string_view>
#include "math_token.hpp"
#include "../lexer/lexer.hpp"
#include "../core/symbols.hpp"

namespace fizmo {
namespace mathtext {

inline std::uint32_t decode_codepoint(std::string_view s, std::size_t i, std::size_t end) noexcept {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const std::size_t n = end - i;
    if (n == 2 && (c & 0xE0) == 0xC0) return ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
    if (n == 3 && (c & 0xF0) == 0xE0) return ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
    if (n == 4 && (c & 0xF8) == 0xF0) return ((c & 0x07u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
    return n == 1 ? c : 0xFFFFFFFFu;
}

class MathLexer {
public:
    explicit MathLexer(std::string_view source, bool newline_separates = false) noexcept : m_src(source), m_newlines(newline_separates) {}

    std::string_view source() const noexcept { return m_src; }

    void reset(std::size_t pos) noexcept {
        m_pos = pos < m_src.size() ? pos : m_src.size();
        m_has_peek = false;
    }

    const MathToken& peek() noexcept {
        if (!m_has_peek) {
            m_peek = scan();
            m_has_peek = true;
        }
        return m_peek;
    }

    MathToken next() noexcept {
        if (m_has_peek) {
            m_has_peek = false;
            return m_peek;
        }
        return scan();
    }

private:
    MathToken make(MathTokenKind k, std::size_t b, std::size_t e, std::uint32_t value = 0) noexcept {
        m_pos = e;
        return MathToken{ k, Span::between(b, e), value };
    }

    std::size_t number_end(std::size_t i) const noexcept {
        const std::size_t n = m_src.size();
        while (i < n && chars::is_digit(m_src[i])) ++i;
        if (i < n && m_src[i] == '.') {
            ++i;
            while (i < n && chars::is_digit(m_src[i])) ++i;
        }
        if (i < n && (m_src[i] == 'e' || m_src[i] == 'E')) {
            std::size_t j = i + 1;
            if (j < n && chars::is_sign(m_src[j])) ++j;
            if (j < n && chars::is_digit(m_src[j])) {
                while (j < n && chars::is_digit(m_src[j])) ++j;
                i = j;
            }
        }
        return i;
    }

    static bool operator_like(const SymbolInfo& info) noexcept {
        return info.cls == SymbolClass::BinaryOperator || info.cls == SymbolClass::Relation || info.cls == SymbolClass::NegatedRelation || info.cls == SymbolClass::Arrow;
    }

    bool continues_at(std::size_t i) const noexcept {
        const char c = m_src[i];
        switch (c) {
            case '+': case '-': case '*': case '/': case '^': case '_': case '=': case '<': case '>': case '!': case ',': case ';': case ')': case '}':
                return true;
            default: break;
        }
        if (c == '\\') {
            const CommandScan cs = scan_command(m_src, i, m_src.size());
            if (cs.shape != CommandShape::Closed) return false;
            const std::uint32_t index = find_symbol(m_src.substr(cs.name_begin, cs.name_end - cs.name_begin));
            return index != kNoSymbol && operator_like(symbol_info(index));
        }
        const std::size_t e = utf8::advance(m_src, i);
        if (e - i > 1) {
            const std::uint32_t cp = decode_codepoint(m_src, i, e);
            if (cp == 0x2212 || cp == 0x00B7) return true;
            const std::uint32_t index = find_symbol_by_codepoint(cp);
            return index != kNoSymbol && operator_like(symbol_info(index));
        }
        return false;
    }

    bool leaves_open(const MathToken& t) const noexcept {
        switch (t.kind) {
            case MathTokenKind::Plus: case MathTokenKind::Minus: case MathTokenKind::Star: case MathTokenKind::Slash:
            case MathTokenKind::Caret: case MathTokenKind::Underscore: case MathTokenKind::Equal: case MathTokenKind::Less:
            case MathTokenKind::Greater: case MathTokenKind::LessEqual: case MathTokenKind::GreaterEqual: case MathTokenKind::NotEqual:
            case MathTokenKind::Comma: case MathTokenKind::Semicolon: case MathTokenKind::LeftParen: case MathTokenKind::LeftBrace:
            case MathTokenKind::End:
                return true;
            case MathTokenKind::Glyph: return operator_like(symbol_info(t.value));
            case MathTokenKind::Command: {
                const std::uint32_t index = find_symbol(m_src.substr(t.span.begin + 1, t.span.length >= 2 ? t.span.length - 2 : 0));
                return index != kNoSymbol && operator_like(symbol_info(index));
            }
            default: return false;
        }
    }

    MathToken scan() noexcept {
        if (m_newlines && m_depth == 0 && !m_open) {
            std::size_t i = m_pos;
            std::size_t newline = m_src.size();
            while (i < m_src.size() && chars::is_space(m_src[i])) {
                if (m_src[i] == '\n' && newline == m_src.size()) newline = i;
                ++i;
            }
            if (newline < m_src.size() && i < m_src.size() && !continues_at(i)) {
                m_open = true;
                m_pos = newline + 1;
                return MathToken{ MathTokenKind::Semicolon, Span(static_cast<std::uint32_t>(newline), 1), 1 };
            }
        }
        const MathToken t = scan_token();
        m_open = leaves_open(t);
        if (t.is(MathTokenKind::LeftParen) || t.is(MathTokenKind::LeftBrace)) ++m_depth;
        else if ((t.is(MathTokenKind::RightParen) || t.is(MathTokenKind::RightBrace)) && m_depth > 0) --m_depth;
        return t;
    }

    MathToken scan_token() noexcept {
        const std::size_t n = m_src.size();
        std::size_t i = m_pos;
        while (i < n && chars::is_space(m_src[i])) ++i;
        if (i >= n) return make(MathTokenKind::End, n, n);
        const char c = m_src[i];
        const char d = i + 1 < n ? m_src[i + 1] : '\0';
        switch (c) {
            case '(': return make(MathTokenKind::LeftParen, i, i + 1);
            case ')': return make(MathTokenKind::RightParen, i, i + 1);
            case '{': return make(MathTokenKind::LeftBrace, i, i + 1);
            case '}': return make(MathTokenKind::RightBrace, i, i + 1);
            case ',': return make(MathTokenKind::Comma, i, i + 1);
            case ';': return make(MathTokenKind::Semicolon, i, i + 1);
            case '+': return make(MathTokenKind::Plus, i, i + 1);
            case '-': return make(MathTokenKind::Minus, i, i + 1);
            case '*': return make(MathTokenKind::Star, i, i + 1);
            case '/': return make(MathTokenKind::Slash, i, i + 1);
            case '^': return make(MathTokenKind::Caret, i, i + 1);
            case '_': return make(MathTokenKind::Underscore, i, i + 1);
            case '=': return make(MathTokenKind::Equal, i, d == '=' ? i + 2 : i + 1);
            case '<': return d == '=' ? make(MathTokenKind::LessEqual, i, i + 2) : make(MathTokenKind::Less, i, i + 1);
            case '>': return d == '=' ? make(MathTokenKind::GreaterEqual, i, i + 2) : make(MathTokenKind::Greater, i, i + 1);
            case '!': return d == '=' ? make(MathTokenKind::NotEqual, i, i + 2) : make(MathTokenKind::Bang, i, i + 1);
            case '"':
            case '\'': {
                bool terminated = false;
                const std::size_t e = scan_quoted(m_src, i, terminated);
                return make(terminated ? MathTokenKind::String : MathTokenKind::BadString, i, e);
            }
            case '\\': {
                const CommandScan cs = scan_command(m_src, i, n);
                if (cs.shape == CommandShape::Closed) return make(MathTokenKind::Command, i, cs.end);
                if (cs.shape == CommandShape::Unclosed) return make(MathTokenKind::UnclosedCommand, i, cs.end);
                return make(MathTokenKind::LoneBackslash, i, cs.end);
            }
            default: break;
        }
        if (chars::is_digit(c) || (c == '.' && chars::is_digit(d))) {
            std::size_t e = number_end(i);
            if (e < n && (m_src[e] == '.' || (m_src[e - 1] == '.' && chars::is_digit(m_src[e])))) {
                while (e < n && (chars::is_digit(m_src[e]) || m_src[e] == '.')) ++e;
                return make(MathTokenKind::BadNumber, i, e);
            }
            return make(MathTokenKind::Number, i, e);
        }
        if (chars::is_letter(c)) {
            std::size_t e = i + 1;
            while (e < n && (chars::is_letter(m_src[e]) || chars::is_digit(m_src[e]))) ++e;
            return make(MathTokenKind::Identifier, i, e);
        }
        const std::size_t e = utf8::advance(m_src, i);
        if (e - i > 1) {
            const std::uint32_t cp = decode_codepoint(m_src, i, e);
            if (cp == 0x2212) return make(MathTokenKind::Minus, i, e);
            if (cp == 0x00B7) return make(MathTokenKind::Star, i, e);
            const std::uint32_t index = find_symbol_by_codepoint(cp);
            if (index != kNoSymbol) return make(MathTokenKind::Glyph, i, e, index);
        }
        return make(MathTokenKind::Unknown, i, e);
    }

    std::string_view m_src;
    std::size_t      m_pos = 0;
    MathToken        m_peek;
    bool             m_has_peek = false;
    bool             m_newlines = false;
    bool             m_open = true;
    std::size_t      m_depth = 0;
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_LEXER_HPP
