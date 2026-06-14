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
    std::uint64_t get_or_create(const std::string& name) {
        auto it = name_to_id_.find(name);
        if (it != name_to_id_.end()) return it->second;
        std::uint64_t id = static_cast<std::uint64_t>(names_.size());
        names_.push_back(name);
        name_to_id_.emplace(name, id);
        return id;
    }

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

    static ComplexNodeKey make_constant(double r, double i) {
        ComplexNodeKey k{};
        k.type = ComplexNodeType::Constant;
        k.constant.real = r;
        k.constant.imag = i;
        return k;
    }

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

    static ComplexNodeKey make_power(ComplexMathExpressionNode* b, ComplexMathExpressionNode* e) {
        ComplexNodeKey k{};
        k.type = ComplexNodeType::Power;
        k.power.base = b;
        k.power.exponent = e;
        return k;
    }

    static ComplexNodeKey make_typed_leaf(ComplexNodeType t) {
        ComplexNodeKey k{};
        k.type = t;
        return k;
    }
};

struct ComplexNodeKeyHash {
    std::size_t operator()(ComplexNodeKey const& k) const noexcept {
        std::size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(k.type));
        auto mix = [&](std::size_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };

        switch (k.type) {
            case ComplexNodeType::Constant: {
                std::uint64_t br, bi;
                std::memcpy(&br, &k.constant.real, sizeof(br));
                std::memcpy(&bi, &k.constant.imag, sizeof(bi));
                mix(br);
                mix(bi);
                break;
            }
            case ComplexNodeType::Variable:
                mix(std::hash<std::uint64_t>{}(k.var_id));
                break;
            case ComplexNodeType::Power:
                mix(std::hash<ComplexMathExpressionNode*>{}(k.power.base));
                mix(std::hash<ComplexMathExpressionNode*>{}(k.power.exponent));
                break;
            default:
                if (is_binary(k.type)) {
                    mix(std::hash<ComplexMathExpressionNode*>{}(k.binary.left));
                    mix(std::hash<ComplexMathExpressionNode*>{}(k.binary.right));
                } else if (is_unary(k.type)) {
                    mix(std::hash<ComplexMathExpressionNode*>{}(k.child));
                }
                break;
        }
        return h;
    }

    static bool is_typed_leaf(ComplexNodeType t) {
        switch (t) {
            case ComplexNodeType::Indeterminate:
            case ComplexNodeType::Infinity:
            case ComplexNodeType::NaN:
            case ComplexNodeType::Undefined:
            case ComplexNodeType::Invalid:
                return true;
            default:
                return false;
        }
    }

    static bool is_binary(ComplexNodeType t) {
        switch (t) {
            case ComplexNodeType::Add:
            case ComplexNodeType::Subtract:
            case ComplexNodeType::Multiply:
            case ComplexNodeType::Divide:
            case ComplexNodeType::Log:
            case ComplexNodeType::Root:
            case ComplexNodeType::Power:
                return true;
            default:
                return false;
        }
    }

    static bool is_unary(ComplexNodeType t) {
        switch (t) {
            case ComplexNodeType::Negate:
            case ComplexNodeType::Sqrt:
            case ComplexNodeType::Cbrt:
            case ComplexNodeType::NaturalExp:
            case ComplexNodeType::NaturalLog:
            case ComplexNodeType::Sin:
            case ComplexNodeType::Cos:
            case ComplexNodeType::Tan:
            case ComplexNodeType::Csc:
            case ComplexNodeType::Sec:
            case ComplexNodeType::Cot:
            case ComplexNodeType::Arcsin:
            case ComplexNodeType::Arccos:
            case ComplexNodeType::Arctan:
            case ComplexNodeType::Arccsc:
            case ComplexNodeType::Arcsec:
            case ComplexNodeType::Arccot:
            case ComplexNodeType::Sinh:
            case ComplexNodeType::Cosh:
            case ComplexNodeType::Tanh:
            case ComplexNodeType::Csch:
            case ComplexNodeType::Sech:
            case ComplexNodeType::Coth:
            case ComplexNodeType::Arcsinh:
            case ComplexNodeType::Arccosh:
            case ComplexNodeType::Arctanh:
            case ComplexNodeType::Arccsch:
            case ComplexNodeType::Arcsech:
            case ComplexNodeType::Arccoth:
            case ComplexNodeType::Conjugate:
            case ComplexNodeType::RealPart:
            case ComplexNodeType::ImaginaryPart:
            case ComplexNodeType::Magnitude:
            case ComplexNodeType::Argument:
            case ComplexNodeType::Reciprocal:
            case ComplexNodeType::Sign:
                return true;
            default:
                return false;
        }
    }
};

struct ComplexNodeKeyEq {
    bool operator()(ComplexNodeKey const& a, ComplexNodeKey const& b) const noexcept {
        if (a.type != b.type) return false;

        switch (a.type) {
            case ComplexNodeType::Constant: {
                std::uint64_t ar, ai, br, bi;
                std::memcpy(&ar, &a.constant.real, sizeof(ar));
                std::memcpy(&ai, &a.constant.imag, sizeof(ai));
                std::memcpy(&br, &b.constant.real, sizeof(br));
                std::memcpy(&bi, &b.constant.imag, sizeof(bi));
                return ar == br && ai == bi;
            }
            case ComplexNodeType::Variable: return a.var_id == b.var_id;
            case ComplexNodeType::Power:    return a.power.base == b.power.base && a.power.exponent == b.power.exponent;
            default:
                if (ComplexNodeKeyHash::is_typed_leaf(a.type)) return true;
                if (ComplexNodeKeyHash::is_binary(a.type)) return a.binary.left == b.binary.left && a.binary.right == b.binary.right;
                if (ComplexNodeKeyHash::is_unary(a.type)) return a.child == b.child;
                return false;
        }
    }
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

    ComplexMathExpression constant(double r, double i = 0.0) {
        ComplexNodeKey key = ComplexNodeKey::make_constant(r, i);

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = ComplexNodeType::Constant;
            n->constant.real = r;
            n->constant.imag = i;
            return n;
        }));
    }

    ComplexMathExpression constant(const BasicComplex<double>& c) { return constant(c.real(), c.imaginary()); }

    ComplexMathExpression variable(const std::string& name) {
        std::uint64_t id = vars_.get_or_create(name);
        ComplexNodeKey key = ComplexNodeKey::make_variable(id);

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = ComplexNodeType::Variable;
            n->variable.var_id = id;
            return n;
        }));
    }

    ComplexMathExpression variable_by_id(std::uint64_t id) {
        ComplexNodeKey key = ComplexNodeKey::make_variable(id);

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = ComplexNodeType::Variable;
            n->variable.var_id = id;
            return n;
        }));
    }

    ComplexMathExpression unary(ComplexNodeType t, ComplexMathExpression a) {
        ComplexNodeKey key = ComplexNodeKey::make_unary(t, a.get());

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = t;
            n->unary.child = a.get();
            return n;
        }));
    }

    ComplexMathExpression binary(ComplexNodeType t, ComplexMathExpression a, ComplexMathExpression b) {
        ComplexNodeKey key = ComplexNodeKey::make_binary(t, a.get(), b.get());

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = t;
            n->binary.left = a.get();
            n->binary.right = b.get();
            return n;
        }));
    }

    ComplexMathExpression power(ComplexMathExpression base, ComplexMathExpression exp) {
        ComplexNodeKey key = ComplexNodeKey::make_power(base.get(), exp.get());

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = ComplexNodeType::Power;
            n->power.base = base.get();
            n->power.exponent = exp.get();
            return n;
        }));
    }

    ComplexMathExpression infinity()           { return typed_leaf(ComplexNodeType::Infinity); }
    ComplexMathExpression nan_expr()           { return typed_leaf(ComplexNodeType::NaN); }
    ComplexMathExpression undefined_expr()     { return typed_leaf(ComplexNodeType::Undefined); }
    ComplexMathExpression indeterminate_expr() { return typed_leaf(ComplexNodeType::Indeterminate); }
    ComplexMathExpression invalid_expr()       { return typed_leaf(ComplexNodeType::Invalid); }

    ComplexMathExpression canonicalize(ComplexMathExpressionNode* n) {
        if (!n) return ComplexMathExpression{};

        switch (n->type) {
            case ComplexNodeType::Constant:      return constant(n->constant.real, n->constant.imag);
            case ComplexNodeType::Variable:      return variable_by_id(n->variable.var_id);
            case ComplexNodeType::Infinity:      return infinity();
            case ComplexNodeType::NaN:           return nan_expr();
            case ComplexNodeType::Undefined:     return undefined_expr();
            case ComplexNodeType::Indeterminate: return indeterminate_expr();
            case ComplexNodeType::Invalid:       return invalid_expr();
            case ComplexNodeType::Power: {
                auto b = canonicalize(n->power.base);
                auto e = canonicalize(n->power.exponent);
                return power(b, e);
            }
            default:
                if (ComplexNodeKeyHash::is_unary(n->type)) {
                    auto child = canonicalize(n->unary.child);
                    return unary(n->type, child);
                }
                if (ComplexNodeKeyHash::is_binary(n->type)) {
                    auto l = canonicalize(n->binary.left);
                    auto r = canonicalize(n->binary.right);
                    return binary(n->type, l, r);
                }
                return invalid_expr();
        }
    }

    const ComplexVariableTable& variables() const noexcept { return vars_; }
    ComplexVariableTable& variables() noexcept { return vars_; }

private:
    ComplexMathExpression typed_leaf(ComplexNodeType t) {
        ComplexNodeKey key = ComplexNodeKey::make_typed_leaf(t);

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = t;
            return n;
        }));
    }

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