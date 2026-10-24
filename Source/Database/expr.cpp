#include "fizmo_library.hpp"
#include "expr.hpp"

#include <cctype>
#include <cmath>
#include <limits>

namespace fizmo {
namespace database {

namespace {

ExprPtr unary(Op op, const Expr& a) { return Expr::make(op, { a.node() }); }
ExprPtr binary(Op op, const Expr& a, const Expr& b) { return Expr::make(op, { a.node(), b.node() }); }

ExprPtr list_node(Op op, const Expr& a, std::vector<Value> values) {
    std::vector<ExprPtr> args{ a.node() };
    for (auto& v : values) args.push_back(Expr::make_literal(std::move(v)));
    return Expr::make(op, std::move(args));
}

char fold(char c) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool add_overflow(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) || (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b)) return true;
    out = a + b;
    return false;
}

bool sub_overflow(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if ((b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) || (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b)) return true;
    out = a - b;
    return false;
}

bool mul_overflow(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if (a == 0 || b == 0) { out = 0; return false; }
    if (a == -1) { if (b == std::numeric_limits<std::int64_t>::min()) return true; out = -b; return false; }
    if (b == -1) { if (a == std::numeric_limits<std::int64_t>::min()) return true; out = -a; return false; }
    const std::int64_t r = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b));
    if (r / b != a) return true;
    out = r;
    return false;
}

} // namespace

const char* to_string(Op op) noexcept {
    switch (op) {
        case Op::Column:    return "column";
        case Op::Literal:   return "literal";
        case Op::Param:     return "param";
        case Op::Eq:        return "=";
        case Op::Ne:        return "!=";
        case Op::Lt:        return "<";
        case Op::Le:        return "<=";
        case Op::Gt:        return ">";
        case Op::Ge:        return ">=";
        case Op::In:        return "IN";
        case Op::NotIn:     return "NOT IN";
        case Op::Between:   return "BETWEEN";
        case Op::IsNull:    return "IS NULL";
        case Op::IsNotNull: return "IS NOT NULL";
        case Op::Like:      return "LIKE";
        case Op::ILike:     return "ILIKE";
        case Op::And:       return "AND";
        case Op::Or:        return "OR";
        case Op::Not:       return "NOT";
        case Op::Add:       return "+";
        case Op::Sub:       return "-";
        case Op::Mul:       return "*";
        case Op::Div:       return "/";
        case Op::Mod:       return "%";
        case Op::Neg:       return "-";
        case Op::Lower:     return "LOWER";
        case Op::Upper:     return "UPPER";
        case Op::Length:    return "LENGTH";
        case Op::Abs:       return "ABS";
        default:            return "COALESCE";
    }
}

bool is_comparison(Op op) noexcept {
    return op == Op::Eq || op == Op::Ne || op == Op::Lt || op == Op::Le || op == Op::Gt || op == Op::Ge;
}

bool is_predicate(Op op) noexcept {
    switch (op) {
        case Op::Eq: case Op::Ne: case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge:
        case Op::In: case Op::NotIn: case Op::Between: case Op::IsNull: case Op::IsNotNull:
        case Op::Like: case Op::ILike: case Op::And: case Op::Or: case Op::Not:
            return true;
        default:
            return false;
    }
}

Op flip_comparison(Op op) noexcept {
    switch (op) {
        case Op::Lt: return Op::Gt;
        case Op::Le: return Op::Ge;
        case Op::Gt: return Op::Lt;
        case Op::Ge: return Op::Le;
        default:     return op;
    }
}

ExprPtr Expr::make(Op op, std::vector<ExprPtr> args) {
    for (const auto& a : args) {
        if (!a) throw DatabaseError(std::string("empty operand for ") + database::to_string(op));
    }

    auto n = std::make_shared<ExprNode>();
    n->op = op;
    n->args = std::move(args);
    return n;
}

ExprPtr Expr::make_literal(Value v) {
    auto n = std::make_shared<ExprNode>();
    n->op = Op::Literal;
    n->value = std::move(v);
    return n;
}

Expr col(const std::string& name) {
    if (name.empty()) throw DatabaseError("empty column name");
    auto n = std::make_shared<ExprNode>();
    n->op = Op::Column;
    n->name = name;
    return Expr(n);
}

Expr lit(Value v) {
    return Expr(Expr::make_literal(std::move(v)));
}

Expr param(std::size_t index) {
    auto n = std::make_shared<ExprNode>();
    n->op = Op::Param;
    n->param = index;
    return Expr(n);
}

Expr Expr::in(std::vector<Value> values) const { return Expr(list_node(Op::In, *this, std::move(values))); }
Expr Expr::not_in(std::vector<Value> values) const { return Expr(list_node(Op::NotIn, *this, std::move(values))); }
Expr Expr::between(Expr lo, Expr hi) const { return Expr(make(Op::Between, { m_node, lo.node(), hi.node() })); }
Expr Expr::is_null() const { return Expr(unary(Op::IsNull, *this)); }
Expr Expr::is_not_null() const { return Expr(unary(Op::IsNotNull, *this)); }
Expr Expr::like(Expr pattern) const { return Expr(binary(Op::Like, *this, pattern)); }
Expr Expr::ilike(Expr pattern) const { return Expr(binary(Op::ILike, *this, pattern)); }
Expr Expr::lower() const { return Expr(unary(Op::Lower, *this)); }
Expr Expr::upper() const { return Expr(unary(Op::Upper, *this)); }
Expr Expr::length() const { return Expr(unary(Op::Length, *this)); }
Expr Expr::abs() const { return Expr(unary(Op::Abs, *this)); }
Expr Expr::coalesce(Expr fallback) const { return Expr(binary(Op::Coalesce, *this, fallback)); }

Expr Expr::starts_with(const std::string& prefix) const {
    std::string pattern;

    for (char c : prefix) {
        if (c == '%' || c == '_' || c == '\\') pattern += '\\';
        pattern += c;
    }

    return like(Expr(std::string(pattern + "%")));
}

Expr operator==(const Expr& a, const Expr& b) { return Expr(binary(Op::Eq, a, b)); }
Expr operator!=(const Expr& a, const Expr& b) { return Expr(binary(Op::Ne, a, b)); }
Expr operator<(const Expr& a, const Expr& b) { return Expr(binary(Op::Lt, a, b)); }
Expr operator<=(const Expr& a, const Expr& b) { return Expr(binary(Op::Le, a, b)); }
Expr operator>(const Expr& a, const Expr& b) { return Expr(binary(Op::Gt, a, b)); }
Expr operator>=(const Expr& a, const Expr& b) { return Expr(binary(Op::Ge, a, b)); }
Expr operator&&(const Expr& a, const Expr& b) { if (!a.node()) return b; if (!b.node()) return a; return Expr(binary(Op::And, a, b)); }
Expr operator||(const Expr& a, const Expr& b) { if (!a.node()) return b; if (!b.node()) return a; return Expr(binary(Op::Or, a, b)); }
Expr operator!(const Expr& a) { return Expr(unary(Op::Not, a)); }
Expr operator+(const Expr& a, const Expr& b) { return Expr(binary(Op::Add, a, b)); }
Expr operator-(const Expr& a, const Expr& b) { return Expr(binary(Op::Sub, a, b)); }
Expr operator*(const Expr& a, const Expr& b) { return Expr(binary(Op::Mul, a, b)); }
Expr operator/(const Expr& a, const Expr& b) { return Expr(binary(Op::Div, a, b)); }
Expr operator%(const Expr& a, const Expr& b) { return Expr(binary(Op::Mod, a, b)); }
Expr operator-(const Expr& a) { return Expr(unary(Op::Neg, a)); }

std::string Expr::to_string() const {
    if (!m_node) return "";
    const ExprNode& n = *m_node;
    auto arg = [&](std::size_t i) { return Expr(n.args[i]).to_string(); };

    switch (n.op) {
        case Op::Column:  return n.name;
        case Op::Literal: return n.value.is_text() ? "'" + n.value.get_text() + "'" : n.value.to_string();
        case Op::Param:   return "?" + std::to_string(n.param);
        case Op::In:
        case Op::NotIn: {
            std::string s = arg(0) + " " + database::to_string(n.op) + " (";
            for (std::size_t i = 1; i < n.args.size(); ++i) s += (i > 1 ? ", " : "") + arg(i);
            return s + ")";
        }
        case Op::Between:   return arg(0) + " BETWEEN " + arg(1) + " AND " + arg(2);
        case Op::IsNull:
        case Op::IsNotNull: return arg(0) + " " + database::to_string(n.op);
        case Op::Not:       return "NOT (" + arg(0) + ")";
        case Op::Neg:       return "-(" + arg(0) + ")";
        case Op::Lower: case Op::Upper: case Op::Length: case Op::Abs:
            return std::string(database::to_string(n.op)) + "(" + arg(0) + ")";
        case Op::Coalesce:  return "COALESCE(" + arg(0) + ", " + arg(1) + ")";
        case Op::And:
        case Op::Or:        return "(" + arg(0) + " " + database::to_string(n.op) + " " + arg(1) + ")";
        default:            return arg(0) + " " + database::to_string(n.op) + " " + arg(1);
    }
}

void Expr::fingerprint(std::string& out) const {
    if (!m_node) { out += '~'; return; }
    const ExprNode& n = *m_node;
    out += static_cast<char>('A' + static_cast<int>(n.op));

    switch (n.op) {
        case Op::Column:
            out += std::to_string(n.name.size());
            out += ':';
            out += n.name;
            break;
        case Op::Literal:
            encode_key_part(out, n.value);
            break;
        case Op::Param:
            out += std::to_string(n.param);
            out += ';';
            break;
        default:
            out += '(';
            for (const auto& a : n.args) Expr(a).fingerprint(out);
            out += ')';
    }
}

Tri tri_and(Tri a, Tri b) noexcept {
    if (a == Tri::False || b == Tri::False) return Tri::False;
    if (a == Tri::True && b == Tri::True) return Tri::True;
    return Tri::Unknown;
}

Tri tri_or(Tri a, Tri b) noexcept {
    if (a == Tri::True || b == Tri::True) return Tri::True;
    if (a == Tri::False && b == Tri::False) return Tri::False;
    return Tri::Unknown;
}

Tri tri_not(Tri a) noexcept {
    if (a == Tri::Unknown) return a;
    return a == Tri::True ? Tri::False : Tri::True;
}

bool like_match(const std::string& text, const std::string& pattern, bool ci, char escape) {
    std::size_t t = 0, p = 0;
    std::size_t star_p = std::string::npos, star_t = 0;
    const std::size_t tn = text.size(), pn = pattern.size();

    while (t < tn) {
        if (p < pn) {
            char pc = pattern[p];

            if (pc == '%') {
                star_p = ++p;
                star_t = t;
                continue;
            }

            bool literal = false;

            if (pc == escape && p + 1 < pn) {
                pc = pattern[p + 1];
                literal = true;
            }

            if ((!literal && pc == '_') || (ci ? fold(pc) == fold(text[t]) : pc == text[t])) {
                p += literal ? 2 : 1;
                ++t;
                continue;
            }
        }

        if (star_p != std::string::npos) {
            p = star_p;
            t = ++star_t;
            continue;
        }

        return false;
    }

    while (p < pn && pattern[p] == '%') ++p;
    return p == pn;
}

bool like_prefix(const std::string& pattern, std::string& prefix, bool& exact, char escape) {
    prefix.clear();
    exact = false;

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const char c = pattern[i];

        if (c == escape && i + 1 < pattern.size()) {
            prefix += pattern[++i];
            continue;
        }

        if (c == '_') return !prefix.empty();

        if (c == '%') {
            bool only_percent = true;
            for (std::size_t j = i; j < pattern.size(); ++j) if (pattern[j] != '%') { only_percent = false; break; }
            exact = only_percent;
            return !prefix.empty();
        }

        prefix += c;
    }

    return !prefix.empty();
}

Tri compare_values(Op op, const Value& a, const Value& b) noexcept {
    if (a.is_null() || b.is_null()) return Tri::Unknown;
    const int c = compare(a, b);

    switch (op) {
        case Op::Eq: return tri_of(c == 0);
        case Op::Ne: return tri_of(c != 0);
        case Op::Lt: return tri_of(c < 0);
        case Op::Le: return tri_of(c <= 0);
        case Op::Gt: return tri_of(c > 0);
        case Op::Ge: return tri_of(c >= 0);
        default:     return Tri::Unknown;
    }
}

Value arithmetic(Op op, const Value& a, const Value& b) {
    if (a.is_null() || b.is_null()) return Value();

    if (op == Op::Add && a.is_text() && b.is_text()) return Value(a.get_text() + b.get_text());
    if (!a.is_number() && !a.is_bool()) throw TypeError(std::string("cannot apply ") + database::to_string(op) + " to " + database::to_string(a.type()));
    if (!b.is_number() && !b.is_bool()) throw TypeError(std::string("cannot apply ") + database::to_string(op) + " to " + database::to_string(b.type()));

    const bool ints = !a.is_real() && !b.is_real();

    if (ints) {
        const std::int64_t x = a.as_int(), y = b.as_int();
        std::int64_t r = 0;

        switch (op) {
            case Op::Add: if (!add_overflow(x, y, r)) return Value(r); break;
            case Op::Sub: if (!sub_overflow(x, y, r)) return Value(r); break;
            case Op::Mul: if (!mul_overflow(x, y, r)) return Value(r); break;
            case Op::Div:
                if (y == 0) return Value();
                if (x == std::numeric_limits<std::int64_t>::min() && y == -1) break;
                return Value(x / y);
            case Op::Mod:
                if (y == 0) return Value();
                if (y == -1) return Value(0);
                return Value(x % y);
            default: break;
        }
    }

    const double x = a.as_real(), y = b.as_real();

    switch (op) {
        case Op::Add: return Value(x + y);
        case Op::Sub: return Value(x - y);
        case Op::Mul: return Value(x * y);
        case Op::Div: return y == 0.0 ? Value() : Value(x / y);
        case Op::Mod: return y == 0.0 ? Value() : Value(std::fmod(x, y));
        default: throw DatabaseError("not an arithmetic operator");
    }
}

Value apply_function(Op op, const std::vector<Value>& args) {
    const Value& a = args.at(0);

    switch (op) {
        case Op::Neg:
            if (a.is_null()) return a;
            if (a.is_int()) return a.get_int() == std::numeric_limits<std::int64_t>::min() ? Value(-static_cast<double>(a.get_int())) : Value(-a.get_int());
            return Value(-a.as_real());
        case Op::Abs:
            if (a.is_null()) return a;
            if (a.is_int()) return a.get_int() == std::numeric_limits<std::int64_t>::min() ? Value(std::fabs(static_cast<double>(a.get_int()))) : Value(a.get_int() < 0 ? -a.get_int() : a.get_int());
            return Value(std::fabs(a.as_real()));
        case Op::Lower:
        case Op::Upper: {
            if (a.is_null()) return a;
            std::string s = a.as_text();
            for (auto& c : s) c = static_cast<char>(op == Op::Lower ? std::tolower(static_cast<unsigned char>(c)) : std::toupper(static_cast<unsigned char>(c)));
            return Value(std::move(s));
        }
        case Op::Length:
            if (a.is_null()) return a;
            if (a.is_text()) {
                std::int64_t n = 0;
                for (unsigned char c : a.get_text()) if ((c & 0xC0) != 0x80) ++n;
                return Value(n);
            }
            if (a.is_blob()) return Value(static_cast<std::int64_t>(a.get_blob().bytes.size()));
            return Value(static_cast<std::int64_t>(a.to_string().size()));
        case Op::Coalesce:
            for (const auto& v : args) if (!v.is_null()) return v;
            return Value();
        default:
            throw DatabaseError("not a function");
    }
}

} // namespace database
} // namespace fizmo
