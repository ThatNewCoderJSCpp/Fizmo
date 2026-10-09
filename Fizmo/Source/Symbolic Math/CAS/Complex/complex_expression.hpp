#ifndef FIZMO_COMPLEX_EXPRESSION_HPP
#define FIZMO_COMPLEX_EXPRESSION_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cmath>
#include <cstring>

#include "../arena.hpp"
#include "../../../Complex/complex_class.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace complex_symbols {

enum class ComplexNodeType : std::uint8_t {
    Constant = 0,      
    Variable,          
    Indeterminate,
    Infinity,          
    NaN,
    Undefined,
    Invalid,

    Add,
    Subtract,
    Multiply,
    Divide,
    Negate,             

    Power,              
    Root,             
    Sqrt,              
    Cbrt,              
    NaturalExp,         
    NaturalLog,         
    Log,                

    Sin,
    Cos,
    Tan,
    Csc,
    Sec,
    Cot,

    Arcsin,
    Arccos,
    Arctan,
    Arccsc,
    Arcsec,
    Arccot,

    Sinh,
    Cosh,
    Tanh,
    Csch,
    Sech,
    Coth,

    Arcsinh,
    Arccosh,
    Arctanh,
    Arccsch,
    Arcsech,
    Arccoth,

    Conjugate,          
    RealPart,           
    ImaginaryPart,     
    Magnitude,         
    Argument,           

    Reciprocal,         
    Sign,              
};

class ComplexVariableTable {
public:
    std::uint64_t get_or_create(const std::string& name);

    std::uint64_t get(const std::string& name) const {
        auto it = name_to_id_.find(name);
        return it == name_to_id_.end() ? invalid_id : it->second;
    }

    const std::string& name(std::uint64_t id) const { return names_[id]; }
    std::size_t size() const { return names_.size(); }
    static constexpr std::uint64_t invalid_id = UINT32_MAX;

private:
    std::vector<std::string> names_;
    std::unordered_map<std::string, std::uint64_t> name_to_id_;
};

struct ComplexMathExpressionNode {
    ComplexNodeType type;

    union {
        struct {
            double real;
            double imag;
        } constant;

        struct {
            std::uint64_t var_id;
        } variable;

        struct {
            ComplexMathExpressionNode* left;
            ComplexMathExpressionNode* right;
        } binary;

        struct {
            ComplexMathExpressionNode* child;
        } unary;

        struct {
            ComplexMathExpressionNode* base;
            ComplexMathExpressionNode* exponent;
        } power;
    };
};

inline bool is_invalid(const ComplexMathExpressionNode* n) noexcept {
    return n == nullptr || n->type == ComplexNodeType::Invalid;
}

struct ComplexNodeKey {
    ComplexNodeType type;

    union {
        struct {
            double real;
            double imag;
        } constant;
        std::uint64_t var_id;
        struct { ComplexMathExpressionNode* left; ComplexMathExpressionNode* right; } binary;
        ComplexMathExpressionNode* child;
        struct { ComplexMathExpressionNode* base; ComplexMathExpressionNode* exponent; } power;
    };

    static ComplexNodeKey make_constant(double r, double i);

    static ComplexNodeKey make_variable(std::uint64_t id) {
        ComplexNodeKey k{};
        k.type = ComplexNodeType::Variable;
        k.var_id = id;
        return k;
    }

    static ComplexNodeKey make_binary(ComplexNodeType t, ComplexMathExpressionNode* l, ComplexMathExpressionNode* r) {
        ComplexNodeKey k{};
        k.type = t;
        k.binary.left = l;
        k.binary.right = r;
        return k;
    }

    static ComplexNodeKey make_unary(ComplexNodeType t, ComplexMathExpressionNode* c) {
        ComplexNodeKey k{};
        k.type = t;
        k.child = c;
        return k;
    }

    static ComplexNodeKey make_power(ComplexMathExpressionNode* b, ComplexMathExpressionNode* e);

    static ComplexNodeKey make_typed_leaf(ComplexNodeType t) {
        ComplexNodeKey k{};
        k.type = t;
        return k;
    }
};

struct ComplexNodeKeyHash {
    std::size_t operator()(ComplexNodeKey const& k) const noexcept;

    static bool is_typed_leaf(ComplexNodeType t);

    static bool is_binary(ComplexNodeType t);

    static bool is_unary(ComplexNodeType t);
};

struct ComplexNodeKeyEq {
    bool operator()(ComplexNodeKey const& a, ComplexNodeKey const& b) const noexcept;
};

class ComplexMathExpression {
public:
    ComplexMathExpression() : node_(nullptr) {}
    explicit ComplexMathExpression(ComplexMathExpressionNode* n) : node_(n) {}

    ComplexMathExpressionNode* get() const noexcept { return node_; }
    explicit operator bool() const noexcept { return node_ != nullptr; }

private:
    ComplexMathExpressionNode* node_;
};

inline bool is_invalid(ComplexMathExpression e) noexcept { return is_invalid(e.get()); }

class ComplexMathExpressionManager {
public:
    ComplexMathExpressionManager(detail::ExpressionArena& arena, ComplexVariableTable& vars) : arena_(arena), vars_(vars) {}

    ComplexMathExpression constant(double r, double i = 0.0);

    ComplexMathExpression constant(const BasicComplex<double>& c) { return constant(c.real(), c.imaginary()); }

    ComplexMathExpression variable(const std::string& name);

    ComplexMathExpression variable_by_id(std::uint64_t id);

    ComplexMathExpression unary(ComplexNodeType t, ComplexMathExpression a);

    ComplexMathExpression binary(ComplexNodeType t, ComplexMathExpression a, ComplexMathExpression b);

    ComplexMathExpression power(ComplexMathExpression base, ComplexMathExpression exp);

    ComplexMathExpression infinity()           { return typed_leaf(ComplexNodeType::Infinity); }
    ComplexMathExpression nan_expr()           { return typed_leaf(ComplexNodeType::NaN); }
    ComplexMathExpression undefined_expr()     { return typed_leaf(ComplexNodeType::Undefined); }
    ComplexMathExpression indeterminate_expr() { return typed_leaf(ComplexNodeType::Indeterminate); }
    ComplexMathExpression invalid_expr()       { return typed_leaf(ComplexNodeType::Invalid); }

    ComplexMathExpression canonicalize(ComplexMathExpressionNode* n);

    const ComplexVariableTable& variables() const noexcept { return vars_; }
    ComplexVariableTable& variables() noexcept { return vars_; }

private:
    ComplexMathExpression typed_leaf(ComplexNodeType t);

    template <class F>
    ComplexMathExpressionNode* intern(ComplexNodeKey const& key, F&& create) {
        auto it = table_.find(key);
        if (it != table_.end()) return it->second;
        ComplexMathExpressionNode* node = create();
        table_.emplace(key, node);
        return node;
    }

    detail::ExpressionArena& arena_;
    ComplexVariableTable& vars_;
    std::unordered_map<ComplexNodeKey, ComplexMathExpressionNode*, ComplexNodeKeyHash, ComplexNodeKeyEq> table_;
};

enum class ComplexMissingVariablePolicy : std::uint8_t {
    UseDefaultValue,
    ReturnNaN,
    ThrowException
};

struct ComplexEvaluationPolicy {
    ComplexMissingVariablePolicy missing_in_table = ComplexMissingVariablePolicy::UseDefaultValue;
    ComplexMissingVariablePolicy missing_in_input = ComplexMissingVariablePolicy::UseDefaultValue;
    BasicComplex<double> default_value = BasicComplex<double>(0.0, 0.0);
    int branch = 0; 
};

struct ComplexEvalContext {
    std::vector<BasicComplex<double>> values;
    std::vector<bool>                 assigned;
    ComplexEvaluationPolicy           policy;

    void resize(std::size_t n) {
        values.resize(n);
        assigned.assign(n, false);
    }
};

} // namespace complex_symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_COMPLEX_EXPRESSION_HPP