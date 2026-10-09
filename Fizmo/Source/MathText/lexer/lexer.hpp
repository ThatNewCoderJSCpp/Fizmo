#ifndef FIZMO_MATHTEXT_LEXER_HPP
#define FIZMO_MATHTEXT_LEXER_HPP

#include <cstddef>
#include <string_view>
#include "token.hpp"
#include "chars.hpp"
#include "number_scan.hpp"
#include "../core/utf8.hpp"
#include "command_scan.hpp"

namespace fizmo {
namespace mathtext {

inline std::size_t scan_quoted(std::string_view s, std::size_t open, bool& terminated) noexcept {
    const char quote = s[open];
    std::size_t i = open + 1;
    const std::size_t n = s.size();
    while (i < n) {
        const char c = s[i];
        if (c == '\\' && is_string_escape(s, i, n)) { i += 2; continue; }
        if (c == '\\') {
            const CommandScan cs = scan_command(s, i, n);
            if (cs.shape == CommandShape::Closed) { i = cs.end; continue; }
            ++i;
            continue;
        }
        if (c == quote) { terminated = true; return i + 1; }
        if (c == '\n') break;
        ++i;
    }
    terminated = false;
    if (i > open + 1 && i <= n && s[i - 1] == '\r') --i;
    return i;
}

class Lexer {
public:
    explicit Lexer(std::string_view source) noexcept : m_src(source) {}

    std::string_view source() const noexcept { return m_src; }
    std::size_t position() const noexcept { return m_has_peek ? m_peek.span.begin : m_pos; }

    void reset(std::size_t pos) noexcept {
        m_pos = pos < m_src.size() ? pos : m_src.size();
        m_has_peek = false;
    }

    const Token& peek() noexcept {
        if (!m_has_peek) {
            m_peek = scan();
            m_has_peek = true;
        }
        return m_peek;
    }

    Token next() noexcept {
        if (m_has_peek) {
            m_has_peek = false;
            return m_peek;
        }
        return scan();
    }

    std::size_t skip_space(std::size_t i) const noexcept {
        while (i < m_src.size() && chars::is_space(m_src[i])) ++i;
        return i;
    }

private:
    Token make(TokenKind k, std::size_t b, std::size_t e) noexcept {
        m_pos = e;
        return Token{ k, Span::between(b, e) };
    }

    Token scan() noexcept {
        const std::size_t n = m_src.size();
        std::size_t i = skip_space(m_pos);
        if (i >= n) return make(TokenKind::End, n, n);
        const char c = m_src[i];
        switch (c) {
            case '{': return make(TokenKind::LeftBrace, i, i + 1);
            case '}': return make(TokenKind::RightBrace, i, i + 1);
            case ',': return make(TokenKind::Comma, i, i + 1);
            case '"':
            case '\'': {
                bool terminated = false;
                const std::size_t e = scan_quoted(m_src, i, terminated);
                return make(terminated ? TokenKind::String : TokenKind::BadString, i, e);
            }
            case '\\': return command(i);
            default: break;
        }
        if (starts_number(m_src, i)) {
            const std::size_t e = scan_number_run(m_src, i);
            return make(analyze_number(m_src.substr(i, e - i)).valid ? TokenKind::Number : TokenKind::BadNumber, i, e);
        }
        if (chars::is_identifier_start(c)) {
            std::size_t e = i + 1;
            while (e < n && chars::is_identifier_char(m_src[e])) ++e;
            return make(TokenKind::Identifier, i, e);
        }
        if (chars::is_digit(c) || c == '.') {
            const std::size_t e = scan_number_run(m_src, i);
            return make(TokenKind::BadNumber, i, e > i ? e : i + 1);
        }
        return make(TokenKind::Unknown, i, utf8::advance(m_src, i));
    }

    Token command(std::size_t i) noexcept {
        const CommandScan cs = scan_command(m_src, i, m_src.size());
        if (cs.shape == CommandShape::Closed) return make(TokenKind::Command, i, cs.end);
        if (cs.shape == CommandShape::Unclosed) return make(TokenKind::UnclosedCommand, i, cs.end);
        return make(TokenKind::LoneBackslash, i, cs.end);
    }

    std::string_view m_src;
    std::size_t      m_pos = 0;
    Token            m_peek;
    bool             m_has_peek = false;
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_LEXER_HPP
