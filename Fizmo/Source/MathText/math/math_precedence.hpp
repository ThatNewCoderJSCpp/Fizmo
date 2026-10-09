#ifndef FIZMO_MATHTEXT_MATH_PRECEDENCE_HPP
#define FIZMO_MATHTEXT_MATH_PRECEDENCE_HPP

#include <string_view>
#include "math_names.hpp"
#include "../parser/document.hpp"

namespace fizmo {
namespace mathtext {

inline bool is_relation_name(std::string_view nm) noexcept {
    return nm == names::Equals || nm == names::Less || nm == names::Greater || nm == names::LessEqual || nm == names::GreaterEqual || nm == names::NotEqual;
}

inline bool is_named(const Document& doc, NodeId id, std::string_view canonical) noexcept {
    const Node& n = doc.node(id);
    return n.is(NodeKind::Call) && !n.has(node_flags::Command) && canonical_call(doc.name(id)) == canonical;
}

inline std::string_view symbol_canonical(const Document& doc, NodeId id) noexcept {
    const SymbolInfo* info = doc.symbol(id);
    return info ? std::string_view(info->name()) : doc.name(id);
}

inline int math_precedence(const Document& doc, NodeId id) noexcept {
    const Node& n = doc.node(id);
    switch (n.kind) {
        case NodeKind::Number: {
            const std::string_view m = doc.text(n.name);
            return n.has(node_flags::Negative) || (!m.empty() && (m[0] == '-' || m[0] == '+')) ? precedence::Unary : precedence::Atom;
        }
        case NodeKind::Negate: return precedence::Unary;
        case NodeKind::Call: {
            if (n.has(node_flags::Command)) return precedence::Atom;
            const std::string_view nm = canonical_call(doc.name(id));
            const std::size_t argc = n.child_count;
            if (argc >= 2 && nm == names::Add) return precedence::Additive;
            if (argc >= 2 && nm == names::Multiply) return precedence::Multiplicative;
            if (argc >= 2 && nm == names::ImplicitMultiply) return precedence::Implicit;
            if (argc == 2 && nm == names::Subtract) return precedence::Additive;
            if (argc == 2 && nm == names::Fraction) return precedence::Multiplicative;
            if (argc == 2 && nm == names::Power) return precedence::Power;
            if (argc == 2 && nm == names::Subscript) return precedence::Power + 1;
            if (argc == 1 && nm == names::Factorial) return precedence::Postfix;
            if (argc == 1 && nm == names::Negate) return precedence::Unary;
            if (argc == 2 && is_relation_name(nm)) return precedence::Relation;
            if (argc == 3 && (nm == names::Relation || nm == names::Operator) && doc.kind(doc.children(id)[0]) == NodeKind::Symbol) {
                if (nm == names::Relation) return precedence::Relation;
                return is_additive_symbol(symbol_canonical(doc, doc.children(id)[0])) ? precedence::Additive : precedence::Multiplicative;
            }
            return precedence::Atom;
        }
        default: return precedence::Atom;
    }
}

inline bool is_negative_like(const Document& doc, NodeId id) noexcept { return math_precedence(doc, id) == precedence::Unary; }

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_PRECEDENCE_HPP
