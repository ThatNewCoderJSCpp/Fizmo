#ifndef FIZMO_MATHTEXT_MATH_PARSER_HPP
#define FIZMO_MATHTEXT_MATH_PARSER_HPP

#include <cstddef>
#include <string_view>
#include <vector>
#include "math_lexer.hpp"
#include "math_precedence.hpp"
#include "../parser/builder.hpp"
#include "../parser/parser.hpp"

namespace fizmo {
namespace mathtext {

class MathParser {
public:
    explicit MathParser(const ParseOptions& options = ParseOptions()) noexcept : m_options(options) {}

    Document parse(std::string_view source) {
        Document doc;
        Builder b(doc, source, m_options.max_quoted_in_message);
        m_b = &b;
        m_src = b.source();
        m_lex = MathLexer(m_src, m_options.newline_separates);
        m_depth = 0;
        std::vector<NodeId> items;
        for (;;) {
            const MathToken t = m_lex.peek();
            if (t.is(MathTokenKind::End)) break;
            if (t.is(MathTokenKind::Comma) || t.is(MathTokenKind::Semicolon)) {
                m_lex.next();
                continue;
            }
            items.push_back(item(MathTokenKind::End, true));
        }
        b.finish_root(items);
        m_b = nullptr;
        return doc;
    }

private:
    enum class OpKind : std::uint8_t { None, Binary, Nary, Postfix, Implicit };

    struct Op {
        OpKind           kind = OpKind::None;
        int              lbp = 0;
        bool             right = false;
        bool             symbolic = false;
        std::string_view name;
    };

    std::uint32_t symbol_index(const MathToken& t) const noexcept {
        if (t.is(MathTokenKind::Glyph)) return t.value;
        return find_symbol(command_name(t.span).in(m_src));
    }

    Op symbol_op(const MathToken& t) const noexcept {
        Op op;
        const std::uint32_t index = symbol_index(t);
        if (index == kNoSymbol) return op;
        const SymbolInfo& info = symbol_info(index);
        const std::string_view canonical = canonical_relation(info.name());
        if (!canonical.empty()) return Op{ OpKind::Binary, precedence::Relation, false, false, canonical };
        if (is_relation_class(info.cls)) return Op{ OpKind::Binary, precedence::Relation, false, true, names::Relation };
        if (info.cls == SymbolClass::BinaryOperator) return Op{ OpKind::Binary, is_additive_symbol(info.name()) ? precedence::Additive : precedence::Multiplicative, false, true, names::Operator };
        return op;
    }

    Op classify(const MathToken& t) const noexcept {
        switch (t.kind) {
            case MathTokenKind::Plus: return Op{ OpKind::Nary, precedence::Additive, false, false, names::Add };
            case MathTokenKind::Minus: return Op{ OpKind::Binary, precedence::Additive, false, false, names::Subtract };
            case MathTokenKind::Star: return Op{ OpKind::Nary, precedence::Multiplicative, false, false, names::Multiply };
            case MathTokenKind::Slash: return Op{ OpKind::Binary, precedence::Multiplicative, false, false, names::Fraction };
            case MathTokenKind::Caret: return Op{ OpKind::Binary, precedence::Power, true, false, names::Power };
            case MathTokenKind::Underscore: return Op{ OpKind::Binary, precedence::Power, false, false, names::Subscript };
            case MathTokenKind::Bang: return Op{ OpKind::Postfix, precedence::Postfix, false, false, names::Factorial };
            case MathTokenKind::Equal: return Op{ OpKind::Binary, precedence::Relation, false, false, names::Equals };
            case MathTokenKind::Less: return Op{ OpKind::Binary, precedence::Relation, false, false, names::Less };
            case MathTokenKind::Greater: return Op{ OpKind::Binary, precedence::Relation, false, false, names::Greater };
            case MathTokenKind::LessEqual: return Op{ OpKind::Binary, precedence::Relation, false, false, names::LessEqual };
            case MathTokenKind::GreaterEqual: return Op{ OpKind::Binary, precedence::Relation, false, false, names::GreaterEqual };
            case MathTokenKind::NotEqual: return Op{ OpKind::Binary, precedence::Relation, false, false, names::NotEqual };
            case MathTokenKind::Command:
            case MathTokenKind::Glyph: {
                const Op op = symbol_op(t);
                if (op.kind != OpKind::None) return op;
                return Op{ OpKind::Implicit, precedence::Implicit, false, false, names::ImplicitMultiply };
            }
            case MathTokenKind::Number:
            case MathTokenKind::BadNumber:
            case MathTokenKind::Identifier:
            case MathTokenKind::String:
            case MathTokenKind::BadString:
            case MathTokenKind::LeftParen:
            case MathTokenKind::LeftBrace:
            case MathTokenKind::LoneBackslash:
            case MathTokenKind::UnclosedCommand:
            case MathTokenKind::Unknown:
                return Op{ OpKind::Implicit, precedence::Implicit, false, false, names::ImplicitMultiply };
            default: return Op{};
        }
    }

    bool is_operator_token(const MathToken& t) const noexcept {
        const Op op = classify(t);
        return op.kind == OpKind::Binary || op.kind == OpKind::Nary || op.kind == OpKind::Postfix;
    }

    Span cover(NodeId a, NodeId b) const noexcept { return Span::between(m_b->node(a).span.begin, m_b->node(b).span.end()); }

    NodeId juxtapose(NodeId a, NodeId b) {
        const NodeId kids[2] = { a, b };
        return m_b->synthetic_call(names::ImplicitMultiply, cover(a, b), kids, 2);
    }

    NodeId unwrap(NodeId id) const noexcept {
        const Document& d = m_b->document();
        while (d.is_call(id, names::Parens) && d.node(id).child_count == 1 && !d.node(id).has(node_flags::Protected)) id = d.children(id)[0];
        return id;
    }

    void flatten_first(std::vector<NodeId>& operands, std::string_view name) const {
        const Document& d = m_b->document();
        const NodeId first = operands.front();
        if (!d.is_call(first, name) || d.node(first).child_count < 2) return;
        const NodeRange inner = d.children(first);
        operands.erase(operands.begin());
        operands.insert(operands.begin(), inner.begin(), inner.end());
    }

    NodeId need(NodeId id, int required, bool after_juxtaposition = false) const noexcept {
        const Document& d = m_b->document();
        if (!d.is_call(id, names::Parens) || d.node(id).child_count != 1) return id;
        const NodeId inner = d.children(id)[0];
        if (math_precedence(d, inner) < required || (after_juxtaposition && is_negative_like(d, inner))) return inner;
        return id;
    }

    NodeId op_symbol(const MathToken& t) {
        if (t.is(MathTokenKind::Glyph)) return m_b->symbol_by_index(t.span, t.value);
        return m_b->symbol(t.span, command_name(t.span), true);
    }

    NodeId stray(const MathToken& t) {
        m_lex.next();
        DiagnosticCode code = DiagnosticCode::UnexpectedToken;
        if (is_operator_token(t)) code = DiagnosticCode::UnexpectedOperator;
        return m_b->error(t.span, code);
    }

    NodeId item(MathTokenKind close, bool top) {
        NodeId e = expression(0);
        if (e == kNoNode) e = stray(m_lex.peek());
        for (;;) {
            const MathToken t = m_lex.peek();
            if (t.is(MathTokenKind::End) || t.is(MathTokenKind::Comma) || t.kind == close) break;
            if (top && t.is(MathTokenKind::Semicolon)) break;
            NodeId more = expression(0);
            if (more == kNoNode) more = stray(m_lex.peek());
            e = juxtapose(e, more);
        }
        return e;
    }

    bool list(MathTokenKind close, std::vector<NodeId>& items, MathToken& closer) {
        bool expect_item = true;
        for (;;) {
            const MathToken t = m_lex.peek();
            if (t.kind == close) {
                closer = m_lex.next();
                return true;
            }
            if (t.is(MathTokenKind::End)) return false;
            if (t.is(MathTokenKind::Comma)) {
                m_lex.next();
                if (expect_item) m_b->diagnose(DiagnosticCode::EmptyArgument, t.span);
                expect_item = true;
                continue;
            }
            items.push_back(item(close, false));
            expect_item = false;
        }
    }

    NodeId unclosed(std::size_t start, Span opener, DiagnosticCode code, const Builder::Mark& m) {
        m_b->rollback(m);
        m_lex.reset(m_src.size());
        return m_b->error(start, m_src.size(), code, opener);
    }

    NodeId call_args(const MathToken& name, bool command, bool synthetic_glyph, const Builder::Mark& m) {
        const MathToken open = m_lex.next();
        const MathTokenKind close = open.is(MathTokenKind::LeftParen) ? MathTokenKind::RightParen : MathTokenKind::RightBrace;
        std::vector<NodeId> args;
        MathToken closer;
        if (!list(close, args, closer)) return unclosed(name.span.begin, Span::between(name.span.begin, open.span.end()), close == MathTokenKind::RightParen ? DiagnosticCode::UnclosedParenthesis : DiagnosticCode::UnclosedCall, m);
        for (NodeId& a : args) a = unwrap(a);
        const Span whole = Span::between(name.span.begin, closer.span.end());
        if (synthetic_glyph) {
            const NodeId id = m_b->synthetic_call(symbol_info(name.value).name(), whole, args);
            m_b->node(id).flags |= node_flags::Command;
            return id;
        }
        if (command) check_command_call(name.span);
        return m_b->call(whole, command ? command_name(name.span) : name.span, command, args);
    }

    void check_command_call(Span name) {
        const std::string_view nm = command_name(name).in(m_src);
        if (find_symbol(nm) == kNoSymbol && !is_call_spelling(nm)) m_b->diagnose(DiagnosticCode::UnknownCommand, name);
    }

    bool adjacent_paren(const MathToken& t) noexcept {
        const MathToken& p = m_lex.peek();
        return p.is(MathTokenKind::LeftParen) && p.span.begin == t.span.end();
    }

    NodeId negate(const MathToken& minus, NodeId child) {
        Node& c = m_b->node(child);
        const Span whole = Span::between(minus.span.begin, c.span.end());
        if (c.is(NodeKind::Number) && !c.has(node_flags::Negative) && m_src[c.name.begin] != '-' && m_src[c.name.begin] != '+') {
            c.flags |= node_flags::Negative;
            c.span = whole;
            return child;
        }
        if (c.is(NodeKind::Call) || c.is(NodeKind::Symbol) || c.is(NodeKind::String)) return m_b->negate(whole, child);
        return m_b->synthetic_call(names::Negate, whole, &child, 1);
    }

    bool starts_operand(const MathToken& t) noexcept {
        switch (t.kind) {
            case MathTokenKind::Number: case MathTokenKind::BadNumber: case MathTokenKind::Identifier: case MathTokenKind::String:
            case MathTokenKind::BadString: case MathTokenKind::LeftParen: case MathTokenKind::LeftBrace: case MathTokenKind::Plus:
            case MathTokenKind::Minus: case MathTokenKind::LoneBackslash: case MathTokenKind::UnclosedCommand: case MathTokenKind::Unknown:
                return true;
            case MathTokenKind::Command: case MathTokenKind::Glyph:
                return symbol_op(t).kind == OpKind::None;
            default:
                return false;
        }
    }

    NodeId lone_sign(const MathToken& t) {
        const std::uint32_t index = find_symbol(t.is(MathTokenKind::Plus) ? "plus_sign" : "minus_sign");
        return m_b->symbol_by_index(t.span, index);
    }

    NodeId prefix() {
        const Builder::Mark m = m_b->mark();
        const MathToken t = m_lex.peek();
        const bool script = m_script;
        m_script = false;
        switch (t.kind) {
            case MathTokenKind::Number: m_lex.next(); return m_b->number(t.span);
            case MathTokenKind::String: m_lex.next(); return m_b->string_literal(t.span);
            case MathTokenKind::BadNumber: m_lex.next(); return m_b->error(t.span, DiagnosticCode::InvalidNumber);
            case MathTokenKind::BadString: m_lex.next(); return m_b->error(t.span, DiagnosticCode::UnterminatedString);
            case MathTokenKind::LoneBackslash: m_lex.next(); return m_b->error(t.span, DiagnosticCode::LoneBackslash);
            case MathTokenKind::UnclosedCommand: m_lex.next(); return m_b->error(t.span, DiagnosticCode::UnclosedCommand);
            case MathTokenKind::Unknown: m_lex.next(); return m_b->error(t.span, DiagnosticCode::UnexpectedCharacter);
            case MathTokenKind::Identifier:
                m_lex.next();
                if (adjacent_paren(t) || m_lex.peek().is(MathTokenKind::LeftBrace)) return call_args(t, false, false, m);
                return m_b->variable(t.span);
            case MathTokenKind::Command:
            case MathTokenKind::Glyph: {
                if (symbol_op(t).kind != OpKind::None) return kNoNode;
                m_lex.next();
                const bool glyph = t.is(MathTokenKind::Glyph);
                if (adjacent_paren(t) || m_lex.peek().is(MathTokenKind::LeftBrace)) return call_args(t, true, glyph, m);
                return op_symbol(t);
            }
            case MathTokenKind::LeftParen: {
                m_lex.next();
                std::vector<NodeId> items;
                MathToken closer;
                if (!list(MathTokenKind::RightParen, items, closer)) return unclosed(t.span.begin, t.span, DiagnosticCode::UnclosedParenthesis, m);
                if (items.size() == 1 && m_b->document().is_call(items[0], names::Parens) && m_b->node(items[0]).child_count == 1) return items[0];
                return m_b->synthetic_call(names::Parens, Span::between(t.span.begin, closer.span.end()), items);
            }
            case MathTokenKind::LeftBrace: {
                m_lex.next();
                std::vector<NodeId> items;
                MathToken closer;
                if (!list(MathTokenKind::RightBrace, items, closer)) return unclosed(t.span.begin, t.span, DiagnosticCode::UnclosedCall, m);
                if (items.size() == 1) {
                    if (m_b->document().is_call(items[0], names::Parens)) m_b->node(items[0]).flags |= node_flags::Protected;
                    return items[0];
                }
                return m_b->synthetic_call(names::Parens, Span::between(t.span.begin, closer.span.end()), items);
            }
            case MathTokenKind::Minus:
            case MathTokenKind::Plus: {
                m_lex.next();
                const MathToken& after = m_lex.peek();
                if (!starts_operand(after) || (script && (after.is(MathTokenKind::Plus) || after.is(MathTokenKind::Minus)) && after.span.begin > t.span.end())) return lone_sign(t);
                const NodeId operand = expression(precedence::Unary);
                if (operand == kNoNode) return m_b->error(t.span, DiagnosticCode::MissingOperand);
                if (t.is(MathTokenKind::Plus)) return operand;
                return negate(t, need(operand, precedence::Unary));
            }
            default: return kNoNode;
        }
    }

    NodeId expression(int min_bp) {
        if (m_depth >= m_options.max_depth) {
            const MathToken t = m_lex.peek();
            if (t.is(MathTokenKind::End)) return kNoNode;
            m_lex.reset(m_src.size());
            return m_b->error(t.span.begin, m_src.size(), DiagnosticCode::NestingTooDeep, t.span);
        }
        ++m_depth;
        NodeId lhs = prefix();
        if (lhs == kNoNode) {
            --m_depth;
            return kNoNode;
        }
        for (;;) {
            const MathToken t = m_lex.peek();
            const Op op = classify(t);
            if (op.kind == OpKind::None || op.lbp < min_bp) break;
            if (op.kind == OpKind::Postfix) {
                m_lex.next();
                const NodeId operand = need(lhs, precedence::Postfix);
                lhs = m_b->synthetic_call(op.name, Span::between(m_b->node(lhs).span.begin, t.span.end()), &operand, 1);
                continue;
            }
            if (op.kind == OpKind::Implicit) {
                std::vector<NodeId> factors{ lhs };
                while (classify(m_lex.peek()).kind == OpKind::Implicit) {
                    const NodeId rhs = expression(op.lbp + 1);
                    if (rhs == kNoNode) break;
                    factors.push_back(rhs);
                }
                const Span whole = cover(factors.front(), factors.back());
                for (std::size_t i = 0; i < factors.size(); ++i) factors[i] = need(factors[i], i ? op.lbp + 1 : op.lbp, i != 0);
                flatten_first(factors, op.name);
                lhs = m_b->synthetic_call(op.name, whole, factors);
                continue;
            }
            if (op.kind == OpKind::Nary) {
                std::vector<NodeId> operands{ lhs };
                NodeId dangling = kNoNode;
                for (;;) {
                    const MathToken o = m_lex.peek();
                    if (o.kind != t.kind) break;
                    m_lex.next();
                    const NodeId rhs = expression(op.lbp + 1);
                    if (rhs == kNoNode) {
                        dangling = m_b->error(o.span, DiagnosticCode::MissingOperand);
                        break;
                    }
                    operands.push_back(rhs);
                }
                if (operands.size() > 1) {
                    const Span whole = cover(operands.front(), operands.back());
                    for (std::size_t i = 0; i < operands.size(); ++i) operands[i] = need(operands[i], i ? op.lbp + 1 : op.lbp);
                    flatten_first(operands, op.name);
                    lhs = m_b->synthetic_call(op.name, whole, operands);
                }
                if (dangling != kNoNode) lhs = juxtapose(lhs, dangling);
                continue;
            }
            m_lex.next();
            const NodeId sym = op.symbolic ? op_symbol(t) : kNoNode;
            m_script = t.is(MathTokenKind::Caret) || t.is(MathTokenKind::Underscore);
            NodeId rhs = expression(op.right ? op.lbp : op.lbp + 1);
            m_script = false;
            if (rhs == kNoNode) {
                const NodeId err = m_b->error(t.span, DiagnosticCode::MissingOperand);
                lhs = juxtapose(lhs, err);
                continue;
            }
            const Span whole = cover(lhs, rhs);
            if (op.name == names::Fraction) {
                const NodeId kids[2] = { unwrap(lhs), unwrap(rhs) };
                lhs = m_b->synthetic_call(op.name, whole, kids, 2);
            } else if (op.name == names::Power || op.name == names::Subscript) {
                const NodeId kids[2] = { need(lhs, precedence::Power + 1), unwrap(rhs) };
                lhs = m_b->synthetic_call(op.name, whole, kids, 2);
            } else if (op.symbolic) {
                const NodeId kids[3] = { sym, need(lhs, op.lbp), need(rhs, op.lbp + 1) };
                lhs = m_b->synthetic_call(op.name, whole, kids, 3);
            } else {
                const NodeId kids[2] = { need(lhs, op.lbp), need(rhs, op.lbp + 1) };
                lhs = m_b->synthetic_call(op.name, whole, kids, 2);
            }
        }
        --m_depth;
        return lhs;
    }

    ParseOptions     m_options;
    Builder*         m_b = nullptr;
    std::string_view m_src;
    MathLexer        m_lex{ std::string_view() };
    std::size_t      m_depth = 0;
    bool             m_script = false;
};

inline Document parse_math(std::string_view source, const ParseOptions& options = ParseOptions()) { return MathParser(options).parse(source); }

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_PARSER_HPP
