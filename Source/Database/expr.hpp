#ifndef FIZMO_DATABASE_EXPR_HPP
#define FIZMO_DATABASE_EXPR_HPP

#include "value.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace database {

enum class Op : std::uint8_t {
    Column,
    Literal,
    Param,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    In,
    NotIn,
    Between,
    IsNull,
    IsNotNull,
    Like,
    ILike,
    And,
    Or,
    Not,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Neg,
    Lower,
    Upper,
    Length,
    Abs,
    Coalesce
};

const char* to_string(Op op) noexcept;
bool is_comparison(Op op) noexcept;
bool is_predicate(Op op) noexcept;
Op flip_comparison(Op op) noexcept;

struct ExprNode;
using ExprPtr = std::shared_ptr<const ExprNode>;

struct ExprNode {
    Op                   op = Op::Literal;
    std::string          name;
    Value                value;
    std::size_t          param = 0;
    std::vector<ExprPtr> args;
};

class Expr {
public:
    template <typename T>
    struct is_literal {
        using D = typename std::decay<T>::type;
        static constexpr bool value = std::is_arithmetic<D>::value || std::is_same<D, std::string>::value || std::is_same<D, const char*>::value
                                   || std::is_same<D, char*>::value || std::is_same<D, Blob>::value || std::is_same<D, Value>::value || std::is_same<D, std::nullptr_t>::value;
    };

    Expr() = default;
    explicit Expr(ExprPtr node) : m_node(std::move(node)) {}

    template <typename T, typename std::enable_if<is_literal<T>::value, int>::type = 0>
    Expr(T&& v) : m_node(make_literal(Value(std::forward<T>(v)))) {}

    const ExprPtr& node() const noexcept { return m_node; }
    explicit operator bool() const noexcept { return m_node != nullptr; }
    Op op() const noexcept { return m_node ? m_node->op : Op::Literal; }

    Expr in(std::vector<Value> values) const;
    Expr not_in(std::vector<Value> values) const;
    Expr between(Expr lo, Expr hi) const;
    Expr is_null() const;
    Expr is_not_null() const;
    Expr like(Expr pattern) const;
    Expr ilike(Expr pattern) const;
    Expr starts_with(const std::string& prefix) const;
    Expr lower() const;
    Expr upper() const;
    Expr length() const;
    Expr abs() const;
    Expr coalesce(Expr fallback) const;

    std::string to_string() const;
    void fingerprint(std::string& out) const;

    static ExprPtr make(Op op, std::vector<ExprPtr> args);
    static ExprPtr make_literal(Value v);

private:
    ExprPtr m_node;
};

Expr col(const std::string& name);
Expr lit(Value v);
Expr param(std::size_t index);

Expr operator==(const Expr& a, const Expr& b);
Expr operator!=(const Expr& a, const Expr& b);
Expr operator<(const Expr& a, const Expr& b);
Expr operator<=(const Expr& a, const Expr& b);
Expr operator>(const Expr& a, const Expr& b);
Expr operator>=(const Expr& a, const Expr& b);
Expr operator&&(const Expr& a, const Expr& b);
Expr operator||(const Expr& a, const Expr& b);
Expr operator!(const Expr& a);
Expr operator+(const Expr& a, const Expr& b);
Expr operator-(const Expr& a, const Expr& b);
Expr operator*(const Expr& a, const Expr& b);
Expr operator/(const Expr& a, const Expr& b);
Expr operator%(const Expr& a, const Expr& b);
Expr operator-(const Expr& a);

enum class Tri : std::uint8_t {
    False,
    True,
    Unknown
};

inline Tri tri_of(bool b) noexcept { return b ? Tri::True : Tri::False; }
Tri tri_and(Tri a, Tri b) noexcept;
Tri tri_or(Tri a, Tri b) noexcept;
Tri tri_not(Tri a) noexcept;

bool like_match(const std::string& text, const std::string& pattern, bool case_insensitive, char escape = '\\');
bool like_prefix(const std::string& pattern, std::string& prefix, bool& exact, char escape = '\\');

Value arithmetic(Op op, const Value& a, const Value& b);
Value apply_function(Op op, const std::vector<Value>& args);
Tri compare_values(Op op, const Value& a, const Value& b) noexcept;

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_EXPR_HPP
