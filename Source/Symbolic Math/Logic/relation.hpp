#ifndef FIZMO_MATH_LOGIC_HPP
#define FIZMO_MATH_LOGIC_HPP

#include "../Common/main_convenience.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <ostream>
#include <sstream>
#include <cmath>
#include <cstdint>
#include <algorithm>   
#include <utility>     

namespace fizmo {
namespace math {
namespace logic {

enum class RelationOperator : std::uint8_t {
    Equal = 0,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};

inline const char* relation_operator_symbol(RelationOperator op) noexcept {
    switch (op) {
        case RelationOperator::Equal:        return "=";
        case RelationOperator::NotEqual:     return "\u2260";
        case RelationOperator::Less:         return "<";
        case RelationOperator::LessEqual:    return "\u2264";
        case RelationOperator::Greater:      return ">";
        case RelationOperator::GreaterEqual: return "\u2265";
    }
    return "?";
}

inline RelationOperator logical_negate(RelationOperator op) noexcept {
    switch (op) {
        case RelationOperator::Equal:        return RelationOperator::NotEqual;
        case RelationOperator::NotEqual:     return RelationOperator::Equal;
        case RelationOperator::Less:         return RelationOperator::GreaterEqual;
        case RelationOperator::LessEqual:    return RelationOperator::Greater;
        case RelationOperator::Greater:      return RelationOperator::LessEqual;
        case RelationOperator::GreaterEqual: return RelationOperator::Less;
    }
    return op;
}

struct Relation {
    cas::Expression lhs;
    RelationOperator op = RelationOperator::Equal;
    cas::Expression rhs;

    Relation() = default;
    Relation(const cas::Expression& l, RelationOperator o, const cas::Expression& r) : lhs(l), op(o), rhs(r) {}

    cas::Expression residual() const { return lhs - rhs; }

    bool evaluate(const std::unordered_map<std::string, double>& env, double eps = constants::middle_epsilon()) const {
        const double l = lhs.evaluate(env);
        const double r = rhs.evaluate(env);

        switch (op) {
            case RelationOperator::Equal:        return std::abs(l - r) <= eps;
            case RelationOperator::NotEqual:     return std::abs(l - r) >  eps;
            case RelationOperator::Less:         return l <  r;
            case RelationOperator::LessEqual:    return l <= r;
            case RelationOperator::Greater:      return l >  r;
            case RelationOperator::GreaterEqual: return l >= r;
        }
        return false;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << lhs << ' ' << relation_operator_symbol(op) << ' ' << rhs;
        return os.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const Relation& r) {
        return os << r.to_string();
    }
};

enum class BoolOperator : std::uint8_t {
    Not = 0,
    And,
    Or,
    Xor,
    Nand,
    Nor,
    Xnor,
    Implies,
    Iff
};

enum class NumericProperty : std::uint8_t {
    Integer = 0,
    Rational,
    Even,
    Odd,
    Prime,
    Finite
};

namespace detail {

inline bool is_integer_value(double v, double eps) noexcept {
    return std::isfinite(v) && std::abs(v - std::round(v)) <= eps;
}

inline bool integer_value_in_range(double v, double eps, long long& out) noexcept {
    if (!is_integer_value(v, eps)) return false;
    const double r = std::round(v);
    if (std::abs(r) >= 9.0e15) return false;  
    out = static_cast<long long>(r);
    return true;
}

inline bool is_even_value(double v, double eps) noexcept {
    long long n;
    return integer_value_in_range(v, eps, n) && (n % 2) == 0;
}

inline bool is_odd_value(double v, double eps) noexcept {
    long long n;
    return integer_value_in_range(v, eps, n) && (n % 2) != 0;
}

inline bool is_prime_value(double v, double eps) noexcept {
    long long n;
    if (!integer_value_in_range(v, eps, n)) return false;
    if (n < 2)      return false;
    if (n < 4)      return true;            
    if (n % 2 == 0) return false;
    for (long long i = 3; i * i <= n; i += 2) if (n % i == 0) return false;
    return true;
}

inline bool is_rational_value(double v, double eps, long long max_den = 1000000) noexcept {
    if (!std::isfinite(v)) return false;
    long long p0 = 1, p1 = 0; 
    long long q0 = 0, q1 = 1;   
    double r = v;

    for (int i = 0; i < 64; ++i) {
        const double a_d = std::floor(r);
        const long long a = static_cast<long long>(a_d);
        const long long p2 = a * p0 + p1;
        const long long q2 = a * q0 + q1;
        if (q2 <= 0 || q2 > max_den) break;
        p1 = p0; p0 = p2;
        q1 = q0; q0 = q2;
        if (std::abs(static_cast<double>(p0) / static_cast<double>(q0) - v) <= eps) return true;
        const double frac = r - a_d;
        if (std::abs(frac) <= constants::middle_epsilon()) return true;   
        r = 1.0 / frac;
        if (!std::isfinite(r)) break;
    }

    return false;
}

} // namespace detail

class Predicate {
public:
    enum class Kind : std::uint8_t { Const = 0, Atom, Op, Prop };

    Predicate() : kind_(Kind::Const), const_(true) {}
    explicit Predicate(bool value) : kind_(Kind::Const), const_(value) {}
    Predicate(Relation rel) : kind_(Kind::Atom), atom_(std::move(rel)) {}

    static Predicate op_node(BoolOperator op, std::vector<Predicate> args) {
        Predicate p;
        p.kind_ = Kind::Op;
        p.op_   = op;
        p.args_ = std::move(args);
        return p;
    }

    static Predicate property_node(cas::Expression e, NumericProperty prop, bool negated) {
        Predicate p;
        p.kind_      = Kind::Prop;
        p.prop_expr_ = std::move(e);
        p.prop_      = prop;
        p.prop_neg_  = negated;
        return p;
    }

    Kind  kind()                         const noexcept { return kind_;  }
    bool  const_value()                  const noexcept { return const_; }
    const Relation& atom()               const          { return atom_; }
    BoolOperator bool_operator()         const noexcept { return op_;    }
    const std::vector<Predicate>& args() const noexcept { return args_;  }

    const cas::Expression& property_expr()    const          { return prop_expr_; }
    NumericProperty   property()         const noexcept { return prop_;     }
    bool              property_negated() const noexcept { return prop_neg_; }

    bool evaluate(const std::unordered_map<std::string, double>& env, double eps = constants::middle_epsilon()) const {
        switch (kind_) {
            case Kind::Const: return const_;
            case Kind::Atom:  return atom_.evaluate(env, eps);
            case Kind::Prop:  return eval_property(prop_expr_.evaluate(env), eps);
            case Kind::Op:    break;
        }

        auto ev = [&](const Predicate& p) { return p.evaluate(env, eps); };

        switch (op_) {
            case BoolOperator::Not:  return !ev(args_[0]);
            case BoolOperator::And:  for (auto& a : args_) if (!ev(a)) return false; return true;
            case BoolOperator::Or:   for (auto& a : args_) if ( ev(a)) return true;  return false;
            case BoolOperator::Nand: for (auto& a : args_) if (!ev(a)) return true;  return false;
            case BoolOperator::Nor:  for (auto& a : args_) if ( ev(a)) return false; return true;
            case BoolOperator::Xor:  { bool acc=false; for (auto& a:args_) acc^=ev(a); return  acc; }
            case BoolOperator::Xnor: { bool acc=false; for (auto& a:args_) acc^=ev(a); return !acc; }
            case BoolOperator::Implies: return !ev(args_[0]) || ev(args_[1]);
            case BoolOperator::Iff:     return  ev(args_[0]) == ev(args_[1]);
        }

        return false;
    }

    std::vector<std::string> variable_names() const {
        std::vector<std::string> out;
        collect(out);
        return out;
    }

    Predicate simplify() const {
        switch (kind_) {
            case Kind::Const: return *this;
            case Kind::Atom:
                if (atom_.lhs.variable_names().empty() && atom_.rhs.variable_names().empty()) return Predicate(atom_.evaluate({}));
                return *this;
            case Kind::Prop:
                if (prop_expr_.variable_names().empty()) return Predicate(evaluate({}));
                return *this;
            case Kind::Op: break;
        }

        std::vector<Predicate> s;
        s.reserve(args_.size());
        for (auto& a : args_) s.push_back(a.simplify());
        if (op_ == BoolOperator::Not && s[0].kind_ == Kind::Const) return Predicate(!s[0].const_);

        if (op_ == BoolOperator::Not && s[0].kind_ == Kind::Prop) {        
            Predicate r = s[0];
            r.prop_neg_ = !r.prop_neg_;
            return r;
        }

        if (op_ == BoolOperator::And) {
            std::vector<Predicate> kept;
            kept.reserve(s.size());

            for (auto& a : s) {
                if (a.kind_ == Kind::Const) { if (!a.const_) return Predicate(false); }
                else kept.push_back(std::move(a));
            }

            if (kept.empty())     return Predicate(true);
            if (kept.size() == 1) return std::move(kept[0]);
            return op_node(BoolOperator::And, std::move(kept));
        }

        if (op_ == BoolOperator::Or) {
            std::vector<Predicate> kept;
            kept.reserve(s.size());

            for (auto& a : s) {
                if (a.kind_ == Kind::Const) { if (a.const_) return Predicate(true); }
                else kept.push_back(std::move(a));
            }

            if (kept.empty())     return Predicate(false);
            if (kept.size() == 1) return std::move(kept[0]);
            return op_node(BoolOperator::Or, std::move(kept));
        }

        return op_node(op_, std::move(s));
    }

    std::string to_string() const {
        switch (kind_) {
            case Kind::Const: return const_ ? "\u22A4" : "\u22A5";
            case Kind::Atom:  return atom_.to_string();
            case Kind::Prop:  return property_to_string();
            case Kind::Op:    break;
        }

        if (op_ == BoolOperator::Not) return "\u00AC(" + args_[0].to_string() + ")";
        const char* sym = infix_symbol(op_);
        std::string s = "(";

        for (std::size_t i = 0; i < args_.size(); ++i) {
            if (i) s += std::string(" ") + sym + " ";
            s += args_[i].to_string();
        }

        return s + ")";
    }

    friend std::ostream& operator<<(std::ostream& os, const Predicate& p) {
        return os << p.to_string();
    }

private:
    static const char* infix_symbol(BoolOperator op) noexcept {
        switch (op) {
            case BoolOperator::And:     return "\u2227";
            case BoolOperator::Or:      return "\u2228";
            case BoolOperator::Xor:     return "\u2295";
            case BoolOperator::Nand:    return "\u22BC";
            case BoolOperator::Nor:     return "\u22BD";
            case BoolOperator::Xnor:    return "\u2299";
            case BoolOperator::Implies: return "\u2192";
            case BoolOperator::Iff:     return "\u2194";
            default:                    return "?";
        }
    }

    bool eval_property(double v, double eps) const noexcept {
        bool base = false;
        switch (prop_) {
            case NumericProperty::Integer:  base = detail::is_integer_value(v, eps);  break;
            case NumericProperty::Rational: base = detail::is_rational_value(v, eps); break;
            case NumericProperty::Even:     base = detail::is_even_value(v, eps);     break;
            case NumericProperty::Odd:      base = detail::is_odd_value(v, eps);      break;
            case NumericProperty::Prime:    base = detail::is_prime_value(v, eps);    break;
            case NumericProperty::Finite:   base = std::isfinite(v);                  break;
        }
        return prop_neg_ ? !base : base;
    }

    std::string property_to_string() const {
        std::ostringstream os;
        os << prop_expr_;
        const std::string x = os.str();

        switch (prop_) {
            case NumericProperty::Integer:  return x + (prop_neg_ ? " \u2209 \u2124" : " \u2208 \u2124"); 
            case NumericProperty::Rational: return x + (prop_neg_ ? " \u2209 \u211A" : " \u2208 \u211A"); 
            case NumericProperty::Even:     return prop_neg_ ? ("\u00AC(2 \u2223 " + x + ")") : ("2 \u2223 " + x); 
            case NumericProperty::Odd:      return prop_neg_ ? ("\u00AC(2 \u2224 " + x + ")") : ("2 \u2224 " + x);
            case NumericProperty::Prime:    return (prop_neg_ ? std::string("\u00AC") : std::string()) + "prime("  + x + ")";
            case NumericProperty::Finite:   return (prop_neg_ ? std::string("\u00AC") : std::string()) + "finite(" + x + ")";
        }
        return x;
    }

    void collect(std::vector<std::string>& out) const {
        auto add = [&](const std::vector<std::string>& v) {
            for (auto& n : v) if (std::find(out.begin(), out.end(), n) == out.end()) out.push_back(n);
        };

        switch (kind_) {
            case Kind::Const: return;
            case Kind::Atom:  add(atom_.lhs.variable_names()); add(atom_.rhs.variable_names()); return;
            case Kind::Prop:  add(prop_expr_.variable_names()); return;
            case Kind::Op:    for (auto& a : args_) a.collect(out); return;
        }
    }

private:
    Kind kind_;
    bool const_ = false;
    Relation atom_;                 // valid iff kind_ == Atom
    BoolOperator op_ = BoolOperator::And;
    std::vector<Predicate> args_;

    cas::Expression prop_expr_;     // valid iff kind_ == Prop
    NumericProperty prop_ = NumericProperty::Integer;
    bool prop_neg_ = false;
};

namespace detail {
inline std::vector<Predicate> pred_pair(Predicate a, Predicate b) {
    std::vector<Predicate> v;
    v.reserve(2);
    v.push_back(std::move(a));
    v.push_back(std::move(b));
    return v;
}

} // namespace detail

inline Predicate AND (std::vector<Predicate> xs) { return Predicate::op_node(BoolOperator::And,  std::move(xs)); }
inline Predicate OR  (std::vector<Predicate> xs) { return Predicate::op_node(BoolOperator::Or,   std::move(xs)); }
inline Predicate XOR (std::vector<Predicate> xs) { return Predicate::op_node(BoolOperator::Xor,  std::move(xs)); }
inline Predicate NAND(std::vector<Predicate> xs) { return Predicate::op_node(BoolOperator::Nand, std::move(xs)); }
inline Predicate NOR (std::vector<Predicate> xs) { return Predicate::op_node(BoolOperator::Nor,  std::move(xs)); }
inline Predicate XNOR(std::vector<Predicate> xs) { return Predicate::op_node(BoolOperator::Xnor, std::move(xs)); }

inline Predicate NOT(Predicate x) {
    std::vector<Predicate> v;
    v.push_back(std::move(x));
    return Predicate::op_node(BoolOperator::Not, std::move(v));
}

inline Predicate AND    (Predicate a, Predicate b) { return Predicate::op_node(BoolOperator::And,     detail::pred_pair(std::move(a), std::move(b))); }
inline Predicate OR     (Predicate a, Predicate b) { return Predicate::op_node(BoolOperator::Or,      detail::pred_pair(std::move(a), std::move(b))); }
inline Predicate XOR    (Predicate a, Predicate b) { return Predicate::op_node(BoolOperator::Xor,     detail::pred_pair(std::move(a), std::move(b))); }
inline Predicate NAND   (Predicate a, Predicate b) { return Predicate::op_node(BoolOperator::Nand,    detail::pred_pair(std::move(a), std::move(b))); }
inline Predicate NOR    (Predicate a, Predicate b) { return Predicate::op_node(BoolOperator::Nor,     detail::pred_pair(std::move(a), std::move(b))); }
inline Predicate XNOR   (Predicate a, Predicate b) { return Predicate::op_node(BoolOperator::Xnor,    detail::pred_pair(std::move(a), std::move(b))); }
inline Predicate IMPLIES(Predicate a, Predicate b) { return Predicate::op_node(BoolOperator::Implies, detail::pred_pair(std::move(a), std::move(b))); }
inline Predicate IFF    (Predicate a, Predicate b) { return Predicate::op_node(BoolOperator::Iff,     detail::pred_pair(std::move(a), std::move(b))); }

inline Predicate IsPositive   (const cas::Expression& x) { return Relation(x, RelationOperator::Greater,      cas::Const(0.0)); }
inline Predicate IsNegative   (const cas::Expression& x) { return Relation(x, RelationOperator::Less,         cas::Const(0.0)); }
inline Predicate IsNonNegative(const cas::Expression& x) { return Relation(x, RelationOperator::GreaterEqual, cas::Const(0.0)); }
inline Predicate IsNonPositive(const cas::Expression& x) { return Relation(x, RelationOperator::LessEqual,    cas::Const(0.0)); }
inline Predicate IsZero       (const cas::Expression& x) { return Relation(x, RelationOperator::Equal,        cas::Const(0.0)); }
inline Predicate IsNonZero    (const cas::Expression& x) { return Relation(x, RelationOperator::NotEqual,     cas::Const(0.0)); }

inline Predicate IsInteger    (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Integer,  false); }
inline Predicate IsNotInteger (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Integer,  true ); }
inline Predicate IsRational   (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Rational, false); }
inline Predicate IsIrrational (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Rational, true ); }
inline Predicate IsEven       (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Even,     false); }
inline Predicate IsOdd        (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Odd,      false); }
inline Predicate IsPrime      (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Prime,    false); }
inline Predicate IsNotPrime   (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Prime,    true ); }
inline Predicate IsFinite     (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Finite,   false); }
inline Predicate IsInfinite   (const cas::Expression& x) { return Predicate::property_node(x, NumericProperty::Finite,   true ); } // also true for NaN

inline Predicate IsPositiveInteger      (const cas::Expression& x) { return  AND(IsInteger (x), IsPositive          (x)); }
inline Predicate IsNegativeInteger      (const cas::Expression& x) { return  AND(IsInteger (x), IsNegative          (x)); }
inline Predicate IsNonNegativeInteger   (const cas::Expression& x) { return  AND(IsInteger (x), IsNonNegative       (x)); }
inline Predicate IsNonPositiveInteger   (const cas::Expression& x) { return  AND(IsInteger (x), IsNonPositive       (x)); }
inline Predicate IsNotNegativeInteger   (const cas::Expression& x) { return NAND(IsInteger (x), IsNegative          (x)); } 
inline Predicate IsNotNonPositiveInteger(const cas::Expression& x) { return NAND(IsInteger (x), IsNonPositiveInteger(x)); } 
inline Predicate IsNonNegativeOrInteger (const cas::Expression& x) { return NAND(IsNegative(x), IsNotInteger        (x)); }

inline Relation EQUALS(const cas::Expression& a, const cas::Expression& b) { return {            a,  RelationOperator::Equal,            b  }; }
inline Relation EQUALS(const cas::Expression& a, double                 b) { return {            a,  RelationOperator::Equal, cas::Const(b) }; }
inline Relation EQUALS(double                 a, const cas::Expression& b) { return { cas::Const(a), RelationOperator::Equal,            b  }; }
inline Relation EQUALS(double                 a, double                 b) { return { cas::Const(a), RelationOperator::Equal, cas::Const(b) }; }

inline Relation NOT_EQUALS(const cas::Expression& a, const cas::Expression& b) { return {            a,  RelationOperator::NotEqual,            b  }; }
inline Relation NOT_EQUALS(const cas::Expression& a, double                 b) { return {            a,  RelationOperator::NotEqual, cas::Const(b) }; }
inline Relation NOT_EQUALS(double                 a, const cas::Expression& b) { return { cas::Const(a), RelationOperator::NotEqual,            b  }; }
inline Relation NOT_EQUALS(double                 a, double                 b) { return { cas::Const(a), RelationOperator::NotEqual, cas::Const(b) }; }

} // namespace logic
} // namespace math
} // namespace fizmo

inline fizmo::math::logic::Predicate operator&(const fizmo::math::logic::Predicate& a, const fizmo::math::logic::Predicate& b) { return fizmo::math::logic::AND(a, b); }
inline fizmo::math::logic::Predicate operator|(const fizmo::math::logic::Predicate& a, const fizmo::math::logic::Predicate& b) { return fizmo::math::logic::OR (a, b); }
inline fizmo::math::logic::Predicate operator^(const fizmo::math::logic::Predicate& a, const fizmo::math::logic::Predicate& b) { return fizmo::math::logic::XOR(a, b); }
inline fizmo::math::logic::Predicate operator~(const fizmo::math::logic::Predicate& a)                                         { return fizmo::math::logic::NOT(a);    }

inline fizmo::math::logic::Relation operator< (const fizmo::math::cas::Expression& a, const fizmo::math::cas::Expression& b) { return { a, fizmo::math::logic::RelationOperator::Less,         b }; }
inline fizmo::math::logic::Relation operator<=(const fizmo::math::cas::Expression& a, const fizmo::math::cas::Expression& b) { return { a, fizmo::math::logic::RelationOperator::LessEqual,    b }; }
inline fizmo::math::logic::Relation operator> (const fizmo::math::cas::Expression& a, const fizmo::math::cas::Expression& b) { return { a, fizmo::math::logic::RelationOperator::Greater,      b }; }
inline fizmo::math::logic::Relation operator>=(const fizmo::math::cas::Expression& a, const fizmo::math::cas::Expression& b) { return { a, fizmo::math::logic::RelationOperator::GreaterEqual, b }; }

inline fizmo::math::logic::Relation operator< (const fizmo::math::cas::Expression& a, double                              b) { return {                         a,  fizmo::math::logic::RelationOperator::Less,         fizmo::math::cas::Const(b) }; }
inline fizmo::math::logic::Relation operator<=(const fizmo::math::cas::Expression& a, double                              b) { return {                         a,  fizmo::math::logic::RelationOperator::LessEqual,    fizmo::math::cas::Const(b) }; }
inline fizmo::math::logic::Relation operator> (const fizmo::math::cas::Expression& a, double                              b) { return {                         a,  fizmo::math::logic::RelationOperator::Greater,      fizmo::math::cas::Const(b) }; }
inline fizmo::math::logic::Relation operator>=(const fizmo::math::cas::Expression& a, double                              b) { return {                         a,  fizmo::math::logic::RelationOperator::GreaterEqual, fizmo::math::cas::Const(b) }; }
inline fizmo::math::logic::Relation operator< (double                              a, const fizmo::math::cas::Expression& b) { return { fizmo::math::cas::Const(a), fizmo::math::logic::RelationOperator::Less,                                 b  }; }
inline fizmo::math::logic::Relation operator<=(double                              a, const fizmo::math::cas::Expression& b) { return { fizmo::math::cas::Const(a), fizmo::math::logic::RelationOperator::LessEqual,                            b  }; }
inline fizmo::math::logic::Relation operator> (double                              a, const fizmo::math::cas::Expression& b) { return { fizmo::math::cas::Const(a), fizmo::math::logic::RelationOperator::Greater,                              b  }; }
inline fizmo::math::logic::Relation operator>=(double                              a, const fizmo::math::cas::Expression& b) { return { fizmo::math::cas::Const(a), fizmo::math::logic::RelationOperator::GreaterEqual,                         b  }; }

#endif // FIZMO_MATH_LOGIC_HPP