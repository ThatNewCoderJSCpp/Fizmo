#ifndef FIZMO_MATHTEXT_PARSER_HPP
#define FIZMO_MATHTEXT_PARSER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "builder.hpp"
#include "../lexer/lexer.hpp"
#include "../core/names.hpp"

namespace fizmo {
namespace mathtext {

struct ParseOptions {
    std::size_t max_depth = 256;
    std::size_t max_quoted_in_message = 40;
    bool        newline_separates = true;
};

class Parser {
public:
    explicit Parser(const ParseOptions& options = ParseOptions()) noexcept : m_options(options) {}

    Document parse(std::string_view source) {
        Document doc;
        Builder b(doc, source, m_options.max_quoted_in_message);
        m_b = &b;
        m_src = b.source();
        m_lex = Lexer(m_src);
        m_depth = 0;
        std::vector<NodeId> items;
        while (!m_lex.peek().is(TokenKind::End)) items.push_back(parse_value(Context::Top));
        b.finish_root(items);
        m_b = nullptr;
        return doc;
    }

private:
    enum class Context : std::uint8_t { Top, Argument };

    std::size_t skip_error(std::size_t from, Context ctx) const noexcept {
        const std::size_t n = m_src.size();
        std::size_t i = from;
        std::size_t depth = 0;
        while (i < n) {
            const char c = m_src[i];
            if (ctx == Context::Top && depth == 0 && chars::is_space(c)) break;
            if (chars::is_quote(c)) {
                bool terminated = false;
                const std::size_t e = scan_quoted(m_src, i, terminated);
                i = e > i ? e : i + 1;
                continue;
            }
            if (c == '\\') {
                const CommandScan cs = scan_command(m_src, i, n);
                i = cs.end > i ? cs.end : i + 1;
                continue;
            }
            if (c == '{') {
                ++depth;
            } else if (c == '}') {
                if (depth == 0) {
                    if (ctx == Context::Argument) break;
                } else {
                    --depth;
                }
            } else if (c == ',' && depth == 0 && ctx == Context::Argument) {
                break;
            }
            i = utf8::advance(m_src, i);
        }
        return i;
    }

    NodeId fail(const Token& t, DiagnosticCode code, Context ctx) {
        std::size_t e = skip_error(t.span.begin, ctx);
        if (e <= t.span.begin) e = t.span.end() > t.span.begin ? t.span.end() : utf8::advance(m_src, t.span.begin);
        m_lex.reset(e);
        return m_b->error(t.span.begin, e, code, t.span);
    }

    NodeId parse_value(Context ctx) {
        const Token t = m_lex.peek();
        switch (t.kind) {
            case TokenKind::Number: m_lex.next(); return m_b->number(t.span);
            case TokenKind::String: m_lex.next(); return m_b->string_literal(t.span);
            case TokenKind::Command:
                m_lex.next();
                if (m_lex.peek().is(TokenKind::LeftBrace)) return call(t, true, ctx);
                return m_b->symbol(t.span, command_name(t.span), true);
            case TokenKind::Identifier:
                m_lex.next();
                if (m_lex.peek().is(TokenKind::LeftBrace)) return call(t, false, ctx);
                return fail(t, DiagnosticCode::UnquotedText, ctx);
            case TokenKind::BadNumber: return fail(t, DiagnosticCode::InvalidNumber, ctx);
            case TokenKind::BadString: return fail(t, DiagnosticCode::UnterminatedString, ctx);
            case TokenKind::LoneBackslash: return fail(t, DiagnosticCode::LoneBackslash, ctx);
            case TokenKind::UnclosedCommand: return fail(t, DiagnosticCode::UnclosedCommand, ctx);
            case TokenKind::Comma: return fail(t, DiagnosticCode::UnexpectedComma, ctx);
            case TokenKind::RightBrace: return fail(t, DiagnosticCode::UnexpectedCloseBrace, ctx);
            case TokenKind::Unknown:
                if (t.span.length == 1 && m_src[t.span.begin] == '-') return negate(t, ctx);
                return fail(t, DiagnosticCode::UnexpectedCharacter, ctx);
            default: return fail(t, DiagnosticCode::UnexpectedCharacter, ctx);
        }
    }

    NodeId negate(const Token& minus, Context ctx) {
        const Builder::Mark m = m_b->mark();
        m_lex.next();
        const Token inner = m_lex.peek();
        const bool attached = inner.span.begin == minus.span.end();
        const bool operand = inner.is(TokenKind::Command) || inner.is(TokenKind::Identifier) || inner.is(TokenKind::String);
        if (!attached || !operand) {
            m_lex.reset(minus.span.begin);
            return fail(minus, DiagnosticCode::InvalidNegation, ctx);
        }
        const NodeId child = parse_value(ctx);
        if (m_b->node(child).is(NodeKind::Error)) {
            const DiagnosticCode code = m_b->document().diagnostics()[m_b->node(child).value].code;
            const std::size_t e = m_b->node(child).span.end();
            m_b->rollback(m);
            m_lex.reset(e);
            return m_b->error(minus.span.begin, e, code, minus.span);
        }
        return m_b->negate(Span::between(minus.span.begin, m_b->node(child).span.end()), child);
    }

    NodeId argument() {
        const Builder::Mark m = m_b->mark();
        const std::size_t start = m_lex.peek().span.begin;
        const NodeId v = parse_value(Context::Argument);
        const Token t = m_lex.peek();
        if (t.is(TokenKind::Comma) || t.is(TokenKind::RightBrace) || t.is(TokenKind::End)) return v;
        m_b->rollback(m);
        const std::size_t e = skip_error(t.span.begin, Context::Argument);
        m_lex.reset(e);
        return m_b->error(start, e, DiagnosticCode::MissingSeparator, t.span);
    }

    NodeId call(const Token& name, bool command, Context ctx) {
        const Builder::Mark m = m_b->mark();
        const std::size_t start = name.span.begin;
        const Token open = m_lex.next();
        if (m_depth >= m_options.max_depth) {
            std::size_t e = skip_error(start, ctx);
            if (e <= open.span.end()) e = m_src.size();
            m_lex.reset(e);
            return m_b->error(start, e, DiagnosticCode::NestingTooDeep, open.span);
        }
        ++m_depth;
        std::vector<NodeId> args;
        Token close;
        for (;;) {
            const Token t = m_lex.peek();
            if (t.is(TokenKind::RightBrace)) {
                close = m_lex.next();
                break;
            }
            if (t.is(TokenKind::End)) {
                --m_depth;
                m_b->rollback(m);
                m_lex.reset(m_src.size());
                return m_b->error(start, m_src.size(), DiagnosticCode::UnclosedCall, Span::between(start, open.span.end()));
            }
            if (t.is(TokenKind::Comma)) {
                m_lex.next();
                m_b->diagnose(DiagnosticCode::EmptyArgument, t.span);
                continue;
            }
            args.push_back(argument());
            if (m_lex.peek().is(TokenKind::Comma)) m_lex.next();
        }
        --m_depth;
        if (command) {
            const std::string_view nm = command_name(name.span).in(m_src);
            if (find_symbol(nm) == kNoSymbol && !is_call_spelling(nm)) m_b->diagnose(DiagnosticCode::UnknownCommand, name.span);
        }
        return m_b->call(Span::between(start, close.span.end()), command ? command_name(name.span) : name.span, command, args);
    }

    ParseOptions     m_options;
    Builder*         m_b = nullptr;
    std::string_view m_src;
    Lexer            m_lex{ std::string_view() };
    std::size_t      m_depth = 0;
};

inline Document parse(std::string_view source, const ParseOptions& options = ParseOptions()) { return Parser(options).parse(source); }

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_PARSER_HPP
