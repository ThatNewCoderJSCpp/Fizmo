#ifndef FIZMO_EXPRESSION_HPP
#define FIZMO_EXPRESSION_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cmath>
#include <cstring>

#include "arena.hpp"   

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

enum class NodeType {
    Constant = 0,
    Variable,
    Indeterminate,      //  0*inf, inf-inf, 1^inf, etc.
    PositiveInfinity,   
    NegativeInfinity,   
    NaN,                // results in a complex number
    Undefined,          // 1/0, ln(0), etc
    Invalid,            // malformed / unsupported expression 
    AppliedFunction,

    Add,
    Subtract,
    Multiply,
    Divide,
    Negate,
    Modulo,
    AbsoluteValue,
    Floor,
    Ceil,
    Round,
    Truncate,
    FractionalPart,
    IntegerPart,
    Combination, 
    Permutation, 
    Sign, 
    UnitStep, 

    Power,
    Root,
    Sqrt,
    Cbrt,
    NaturalExp,
    NaturalLog,
    Log,

    Sin,
    Sinc,
    NormalSinc,
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

    Erf,
    Erfc,
    ErfGeneralized,     
    ErfcGeneralized,
    InverseErf, 
    InverseErfc, 
    Erfi, 
    InverseErfi,

    AiryAi, // todo
    AiryBi, // todo
    ScorerGi, // todo
    ScorerHi, // todo

    CompleteEllipticK, // todo
    CompleteEllipticE, // todo
    CompleteEllipticPi, // todo
    EllipticF, // todo
    EllipticE, // todo
    EllipticPi, // todo
    CarlsonRF, // todo
    CarlsonRD, // todo
    CarlsonRJ, // todo

    JacobiAm, // todo
    JacobiSn, // todo
    JacobiCn, // todo
    JacobiDn, // todo
    JacobiNs, // todo
    JacobiNc, // todo
    JacobiNd, // todo
    JacobiSc, // todo
    JacobiSd, // todo
    JacobiCs, // todo
    JacobiCd, // todo
    JacobiDs, // todo
    JacobiDc, // todo

    Gamma, 
    Factorial, 
    BetaFunction, 
    Digamma, 
    Trigamma, 
    Polygamma, 
    BellPolynomial, // todo: evaluate and diffrentiate

    RisingPochhammer, // todo
    FallingPochhammer, // todo

    ChebyshevU, 
    ChebyshevT, 

    SinIntegral,
    CosIntegral, 
    SinhIntegral, 
    CoshIntegral, 

    Dilogarithm, 
    Trilogarithm,
    Polylog, // todo: differentiate
    NielsenPolylog, // todo

    LambertW, 

    ExponentialIntegral, 
    LogarithmicIntegral, 
    ExponentialIntegralGeneralized, // todo: differentiate
    LogarithmicIntegralGeneralized, // todo: differentiate

    RogersL, 
    RogersLR, 

    FresnelS, 
    FresnelC, 

    Gudermannian, 
    InverseGudermannian, 

    RiemannZeta, // todo: differentiate
    RiemannZetaDerivative, // todo
    RiemannChi, // todo
    RiemannXi, // todo

    HurwitzZeta, // todo
    LerchTranscendent, // todo

    SpenceFunction, 
    SpenceIntegral, 

    ClausenFunction, // todo
    ClausenIntegral, // todo

    DirichletBeta, // todo
    DirichletEta, // todo
    DirichletLambda, // todo

    LucasSequence, 
    FibonacciSequence, 
    LucasPolynomial, 
    FibonacciPolynomial, 

    LegendreP, // todo
    LegendreQ, // todo
    LegendreChi, // todo

    DebyeInfinite, // todo
    DebyeFinite, // todo

    BesselJ, // todo            
    BesselY, // todo    
    BesselI, // todo           
    BesselK, // todo           

    HankelH1, // todo           
    HankelH2, // todo          

    SphericalBesselJ, // todo   
    SphericalBesselY, // todo  
    SphericalBesselI, // todo  
    SphericalBesselK, // todo  

    GaussianGeneralized, // todo

    UserDefined
};

class VariableTable {
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

struct MathExpressionNode {
    NodeType type;

    union {
        double constant;

        struct {
            std::uint64_t var_id;
        } variable;

        struct {
            MathExpressionNode* left;
            MathExpressionNode* right;
        } binary;

        struct {
            MathExpressionNode* child;
        } unary;

        struct {
            MathExpressionNode* base;
            MathExpressionNode* exponent;
        } power;

        struct {
            MathExpressionNode* arg1;
            MathExpressionNode* arg2;
            MathExpressionNode* arg3;
        } ternary;

        struct {
            MathExpressionNode* arg1;
            MathExpressionNode* arg2;
            MathExpressionNode* arg3;
            MathExpressionNode* arg4;
        } quaternary;

        struct {
            MathExpressionNode** args;       
            std::uint64_t*       orders;     
            std::uint64_t        arg_count;
            std::uint64_t        func_id;
        } applied;
    };
};

inline bool is_invalid(const MathExpressionNode* n) noexcept { return n == nullptr || n->type == NodeType::Invalid; }

struct NodeKey {
    NodeType type;

    union {
        double constant;
        std::uint64_t var_id;
        struct { MathExpressionNode* left; MathExpressionNode* right; } binary;
        MathExpressionNode* child;
        struct { MathExpressionNode* base; MathExpressionNode* exponent; } power;
        struct { MathExpressionNode* arg1; MathExpressionNode* arg2; MathExpressionNode* arg3; } ternary;
        struct { MathExpressionNode* arg1; MathExpressionNode* arg2; MathExpressionNode* arg3; MathExpressionNode* arg4; } quaternary;

        struct {
            MathExpressionNode* const* args;
            const std::uint64_t*       orders;
            std::uint64_t              arg_count;
            std::uint64_t              func_id;
        } applied;
    };

    static NodeKey make_constant(double v) {
        NodeKey k;
        k.type = NodeType::Constant;
        k.constant = v;
        return k;
    }

    static NodeKey make_variable(std::uint64_t id) {
        NodeKey k;
        k.type = NodeType::Variable;
        k.var_id = id;
        return k;
    }

    static NodeKey make_binary(NodeType t, MathExpressionNode* l, MathExpressionNode* r) {
        NodeKey k;
        k.type = t;
        k.binary.left = l;
        k.binary.right = r;
        return k;
    }

    static NodeKey make_unary(NodeType t, MathExpressionNode* c) {
        NodeKey k;
        k.type = t;
        k.child = c;
        return k;
    }

    static NodeKey make_power(MathExpressionNode* b, MathExpressionNode* e) {
        NodeKey k;
        k.type = NodeType::Power;
        k.power.base = b;
        k.power.exponent = e;
        return k;
    }

    static NodeKey make_typed_leaf(NodeType t) {
        NodeKey k{}; 
        k.type = t; 
        return k;
    }

    static NodeKey make_ternary(NodeType t, MathExpressionNode* a1, MathExpressionNode* a2, MathExpressionNode* a3) {
        NodeKey k;
        k.type = t;
        k.ternary.arg1 = a1;
        k.ternary.arg2 = a2;
        k.ternary.arg3 = a3;
        return k;
    }

    static NodeKey make_quaternary(NodeType t, MathExpressionNode* a1, MathExpressionNode* a2, MathExpressionNode* a3, MathExpressionNode* a4);

    static NodeKey make_applied(
        std::uint64_t fid, std::uint64_t count,
        MathExpressionNode* const* args,
        const std::uint64_t* orders
    );
};

struct NodeKeyHash {
    std::size_t operator()(NodeKey const& k) const noexcept;

    static bool is_typed_leaf(NodeType t);

    static bool is_binary(NodeType t);

    static bool is_unary(NodeType t);

    static bool is_ternary(NodeType t) {
        switch (t) {
            default:
                return false;
        }
    }

    static bool is_quaternary(NodeType t) {
        switch (t) {
            default:
                return false;
        }
    }
};

struct NodeKeyEq {
    bool operator()(NodeKey const& a, NodeKey const& b) const noexcept;
};

class MathExpression {
public:
    MathExpression() : node_(nullptr) {}
    explicit MathExpression(MathExpressionNode* n) : node_(n) {}

    MathExpressionNode* get() const noexcept { return node_; }
    explicit operator bool() const noexcept { return node_ != nullptr; }

private:
    MathExpressionNode* node_;
};

inline bool is_invalid(MathExpression e) noexcept { return is_invalid(e.get()); }

class MathExpressionManager {
public:
    MathExpressionManager(detail::ExpressionArena& arena, VariableTable& vars) : arena_(arena), vars_(vars) {}

    MathExpression constant(double v);

    MathExpression variable(const std::string& name);

    MathExpression unary(NodeType t, MathExpression a);

    MathExpression binary(NodeType t, MathExpression a, MathExpression b);

    MathExpression power(MathExpression base, MathExpression exp);

    MathExpression ternary(NodeType t, MathExpression a1, MathExpression a2, MathExpression a3);

    MathExpression quaternary(NodeType t, MathExpression a1, MathExpression a2, MathExpression a3, MathExpression a4);

    MathExpression applied(
        std::uint64_t func_id,
        const std::vector<MathExpression>& args,
        const std::vector<std::uint64_t>& orders
    );

    MathExpression applied(std::uint64_t func_id, std::uint64_t order, MathExpression arg);

    MathExpression pos_inf() { return typed_leaf(NodeType::PositiveInfinity); }
    MathExpression neg_inf() { return typed_leaf(NodeType::NegativeInfinity); }
    MathExpression nan_expr() { return typed_leaf(NodeType::NaN); }
    MathExpression undefined_expr() { return typed_leaf(NodeType::Undefined); }
    MathExpression indeterminate_expr() { return typed_leaf(NodeType::Indeterminate); }
    MathExpression invalid_expr() { return typed_leaf(NodeType::Invalid); }

    MathExpression variable_by_id(std::uint64_t id);

    MathExpression canonicalize(MathExpressionNode* n);

public:
    const VariableTable& variables() const noexcept { return vars_;  }
          VariableTable& variables()       noexcept { return vars_;  }
    const VariableTable& functions() const noexcept { return funcs_; }
          VariableTable& functions()       noexcept { return funcs_; }

    std::uint64_t function_id(const std::string& name) {
        return funcs_.get_or_create(name);
    }

private:
    MathExpression typed_leaf(NodeType t);

    template <class F>
    MathExpressionNode* intern(NodeKey const& key, F&& create) {
        auto it = table_.find(key);
        if (it != table_.end()) return it->second;

        MathExpressionNode* node = create();
        table_.emplace(key, node);
        return node;
    }

    detail::ExpressionArena& arena_;
    VariableTable& vars_;
    VariableTable  funcs_;
    std::unordered_map<NodeKey, MathExpressionNode*, NodeKeyHash, NodeKeyEq> table_;
};

} // namespace symbols

enum class MissingVariablePolicy {
    UseDefaultValue,   
    ReturnNaN,         
    ThrowException    
};

struct EvaluationPolicy {
    MissingVariablePolicy missing_in_table  = MissingVariablePolicy::UseDefaultValue;
    MissingVariablePolicy missing_in_input  = MissingVariablePolicy::UseDefaultValue;
    double default_value = 0.0;
};

namespace symbols {

struct EvalContext {
    std::vector<double> values;
    std::vector<bool>   assigned;
    EvaluationPolicy    policy;

    void resize(std::size_t n) {
        values.resize(n);
        assigned.assign(n, false);
    }
};

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_EXPRESSION_HPP
