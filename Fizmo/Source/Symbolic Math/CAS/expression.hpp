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
    std::uint64_t get_or_create(const std::string& name) {
        auto it = name_to_id_.find(name);
        if (it != name_to_id_.end()) return it->second;
        std::uint64_t id = static_cast<std::uint64_t>(names_.size());
        names_.emplace_back(name);
        name_to_id_.emplace(names_.back(), id);
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

    static NodeKey make_quaternary(NodeType t, MathExpressionNode* a1, MathExpressionNode* a2, MathExpressionNode* a3, MathExpressionNode* a4) {
        NodeKey k;
        k.type = t;
        k.quaternary.arg1 = a1;
        k.quaternary.arg2 = a2;
        k.quaternary.arg3 = a3;
        k.quaternary.arg4 = a4;
        return k;
    }

    static NodeKey make_applied(
        std::uint64_t fid, std::uint64_t count,
        MathExpressionNode* const* args,
        const std::uint64_t* orders
    ) {
        NodeKey k;
        k.type = NodeType::AppliedFunction;
        k.applied.func_id   = fid;
        k.applied.arg_count = count;
        k.applied.args      = args;
        k.applied.orders    = orders;
        return k;
    }
};

struct NodeKeyHash {
    std::size_t operator()(NodeKey const& k) const noexcept {
        std::size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(k.type));
        auto mix = [&](std::size_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };

        switch (k.type) {
            case NodeType::Constant: {
                std::uint64_t bits;
                std::memcpy(&bits, &k.constant, sizeof(bits));
                mix(bits);
                break;
            }

            case NodeType::Variable:
                mix(std::hash<std::uint64_t>{}(k.var_id));
                break;

            case NodeType::Power:
                mix(std::hash<MathExpressionNode*>{}(k.power.base));
                mix(std::hash<MathExpressionNode*>{}(k.power.exponent));
                break;

            case NodeType::AppliedFunction:
                mix(std::hash<std::uint64_t>{}(k.applied.func_id));
                mix(std::hash<std::uint64_t>{}(k.applied.arg_count));

                for (std::uint64_t i = 0; i < k.applied.arg_count; ++i) {
                    mix(std::hash<MathExpressionNode*>{}(k.applied.args[i]));
                    mix(std::hash<std::uint64_t>{}(k.applied.orders[i]));
                }

                break;

            default:
                if (is_quaternary(k.type)) {
                    mix(std::hash<MathExpressionNode*>{}(k.quaternary.arg1));
                    mix(std::hash<MathExpressionNode*>{}(k.quaternary.arg2));
                    mix(std::hash<MathExpressionNode*>{}(k.quaternary.arg3));
                    mix(std::hash<MathExpressionNode*>{}(k.quaternary.arg4));
                } else if (is_ternary(k.type)) {
                    mix(std::hash<MathExpressionNode*>{}(k.ternary.arg1));
                    mix(std::hash<MathExpressionNode*>{}(k.ternary.arg2));
                    mix(std::hash<MathExpressionNode*>{}(k.ternary.arg3));
                } else if (is_binary(k.type)) {
                    mix(std::hash<MathExpressionNode*>{}(k.binary.left));
                    mix(std::hash<MathExpressionNode*>{}(k.binary.right));
                } else if (is_unary(k.type)) {
                    mix(std::hash<MathExpressionNode*>{}(k.child));
                }

                break;
        }

        return h;
    }

    static bool is_typed_leaf(NodeType t) {
        switch (t) {
            case NodeType::Indeterminate:
            case NodeType::PositiveInfinity:
            case NodeType::NegativeInfinity:
            case NodeType::NaN:
            case NodeType::Undefined:
            case NodeType::Invalid:
                return true;
            default:
                return false;
        }
    }

    static bool is_binary(NodeType t) {
        switch (t) {
            case NodeType::Add:
            case NodeType::Subtract:
            case NodeType::Multiply:
            case NodeType::Divide:
            case NodeType::Modulo:
            case NodeType::Log:
            case NodeType::Root:
            case NodeType::Power:
            case NodeType::ErfGeneralized:
            case NodeType::ErfcGeneralized:
            case NodeType::BetaFunction:
            case NodeType::LambertW:
            case NodeType::ChebyshevU:
            case NodeType::ChebyshevT:
            case NodeType::ExponentialIntegralGeneralized:
            case NodeType::LogarithmicIntegralGeneralized:
            case NodeType::BellPolynomial:
            case NodeType::Polygamma:
            case NodeType::Polylog:
            case NodeType::Combination:
            case NodeType::Permutation:
            case NodeType::FibonacciPolynomial:
            case NodeType::LucasPolynomial:
                return true;
            default:
                return false;
        }
    }

    static bool is_unary(NodeType t) {
        switch (t) {
            case NodeType::Negate:
            case NodeType::AbsoluteValue:
            case NodeType::Floor:
            case NodeType::Ceil:
            case NodeType::Round:
            case NodeType::Truncate:
            case NodeType::FractionalPart:
            case NodeType::IntegerPart:
            case NodeType::NaturalExp:
            case NodeType::NaturalLog:
            case NodeType::Sin:
            case NodeType::Sinc:
            case NodeType::NormalSinc:
            case NodeType::Cos:
            case NodeType::Tan:
            case NodeType::Csc:
            case NodeType::Sec:
            case NodeType::Cot:
            case NodeType::Arcsin:
            case NodeType::Arccos:
            case NodeType::Arctan:
            case NodeType::Arccsc:
            case NodeType::Arcsec:
            case NodeType::Arccot:
            case NodeType::Sinh:
            case NodeType::Cosh:
            case NodeType::Tanh:
            case NodeType::Csch:
            case NodeType::Sech:
            case NodeType::Coth:
            case NodeType::Arcsinh:
            case NodeType::Arccosh:
            case NodeType::Arctanh:
            case NodeType::Arccsch:
            case NodeType::Arcsech:
            case NodeType::Arccoth:
            case NodeType::Sqrt:
            case NodeType::Cbrt:
            case NodeType::Erf:
            case NodeType::Erfc:
            case NodeType::Erfi:
            case NodeType::InverseErf:
            case NodeType::InverseErfc:
            case NodeType::InverseErfi:
            case NodeType::Gamma:
            case NodeType::Factorial:
            case NodeType::Digamma:
            case NodeType::Trigamma:
            case NodeType::ExponentialIntegral:
            case NodeType::LogarithmicIntegral:
            case NodeType::SinIntegral:
            case NodeType::CosIntegral:
            case NodeType::SinhIntegral:
            case NodeType::CoshIntegral:
            case NodeType::FresnelS:
            case NodeType::FresnelC:
            case NodeType::RiemannZeta:
            case NodeType::Dilogarithm:
            case NodeType::Trilogarithm:
            case NodeType::SpenceFunction:
            case NodeType::SpenceIntegral:
            case NodeType::RogersL:
            case NodeType::RogersLR:
            case NodeType::Gudermannian:
            case NodeType::InverseGudermannian:
            case NodeType::Sign:
            case NodeType::UnitStep:
            case NodeType::FibonacciSequence:
            case NodeType::LucasSequence:
                return true;
            default:
                return false;
        }
    }

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
    bool operator()(NodeKey const& a, NodeKey const& b) const noexcept {
        if (a.type != b.type) return false;

        switch (a.type) {
            case NodeType::Constant: {
                std::uint64_t ba, bb;
                std::memcpy(&ba, &a.constant, sizeof(ba));
                std::memcpy(&bb, &b.constant, sizeof(bb));
                return ba == bb;
            }

            case NodeType::Variable: return a.var_id == b.var_id;
            case NodeType::Power: return a.power.base == b.power.base && a.power.exponent == b.power.exponent;
            
            case NodeType::AppliedFunction: {
                if (a.applied.func_id   != b.applied.func_id)   return false;
                if (a.applied.arg_count != b.applied.arg_count) return false;
                
                for (std::uint64_t i = 0; i < a.applied.arg_count; ++i) {
                    if (a.applied.args[i]   != b.applied.args[i])   return false;
                    if (a.applied.orders[i] != b.applied.orders[i]) return false;
                }
                return true;
            }

            default:
                if (NodeKeyHash::is_typed_leaf(a.type)) return true;

                if (NodeKeyHash::is_quaternary(a.type)) {
                    return a.quaternary.arg1 == b.quaternary.arg1 &&
                           a.quaternary.arg2 == b.quaternary.arg2 &&
                           a.quaternary.arg3 == b.quaternary.arg3 &&
                           a.quaternary.arg4 == b.quaternary.arg4;
                }

                if (NodeKeyHash::is_ternary(a.type)) {
                    return a.ternary.arg1 == b.ternary.arg1 &&
                           a.ternary.arg2 == b.ternary.arg2 &&
                           a.ternary.arg3 == b.ternary.arg3;
                }

                if (NodeKeyHash::is_binary(a.type)) { return a.binary.left == b.binary.left && a.binary.right == b.binary.right; }
                if (NodeKeyHash::is_unary(a.type)) { return a.child == b.child; }
                return false;
        }
    }
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

    MathExpression constant(double v) {
        NodeKey key = NodeKey::make_constant(v);
        return MathExpression(intern(key, [&] {
            MathExpressionNode* n = arena_.make<MathExpressionNode>();
            n->type = NodeType::Constant;
            n->constant = v;
            return n;
        }));
    }

    MathExpression variable(const std::string& name) {
        std::uint64_t id = vars_.get_or_create(name);
        NodeKey key = NodeKey::make_variable(id);
        return MathExpression(intern(key, [&] {
            MathExpressionNode* n = arena_.make<MathExpressionNode>();
            n->type = NodeType::Variable;
            n->variable.var_id = id;
            return n;
        }));
    }

    MathExpression unary(NodeType t, MathExpression a) {
        NodeKey key = NodeKey::make_unary(t, a.get());
        return MathExpression(intern(key, [&] {
            MathExpressionNode* n = arena_.make<MathExpressionNode>();
            n->type = t;
            n->unary.child = a.get();
            return n;
        }));
    }

    MathExpression binary(NodeType t, MathExpression a, MathExpression b) {
        NodeKey key = NodeKey::make_binary(t, a.get(), b.get());
        return MathExpression(intern(key, [&] {
            MathExpressionNode* n = arena_.make<MathExpressionNode>();
            n->type = t;
            n->binary.left = a.get();
            n->binary.right = b.get();
            return n;
        }));
    }

    MathExpression power(MathExpression base, MathExpression exp) {
        NodeKey key = NodeKey::make_power(base.get(), exp.get());
        return MathExpression(intern(key, [&] {
            MathExpressionNode* n = arena_.make<MathExpressionNode>();
            n->type = NodeType::Power;
            n->power.base = base.get();
            n->power.exponent = exp.get();
            return n;
        }));
    }

    MathExpression ternary(NodeType t, MathExpression a1, MathExpression a2, MathExpression a3) {
        NodeKey key = NodeKey::make_ternary(t, a1.get(), a2.get(), a3.get());
        return MathExpression(intern(key, [&] {
            MathExpressionNode* n = arena_.make<MathExpressionNode>();
            n->type = t;
            n->ternary.arg1 = a1.get();
            n->ternary.arg2 = a2.get();
            n->ternary.arg3 = a3.get();
            return n;
        }));
    }

    MathExpression quaternary(NodeType t, MathExpression a1, MathExpression a2, MathExpression a3, MathExpression a4) {
        NodeKey key = NodeKey::make_quaternary(t, a1.get(), a2.get(), a3.get(), a4.get());
        return MathExpression(intern(key, [&] {
            MathExpressionNode* n = arena_.make<MathExpressionNode>();
            n->type = t;
            n->quaternary.arg1 = a1.get();
            n->quaternary.arg2 = a2.get();
            n->quaternary.arg3 = a3.get();
            n->quaternary.arg4 = a4.get();
            return n;
        }));
    }

    MathExpression applied(
        std::uint64_t func_id,
        const std::vector<MathExpression>& args,
        const std::vector<std::uint64_t>& orders
    ) {
        const std::uint64_t count = static_cast<std::uint64_t>(args.size());
        MathExpressionNode** arg_arr = nullptr;
        std::uint64_t*       ord_arr = nullptr;

        if (count > 0) {
            arg_arr = arena_.allocate_array<MathExpressionNode*>(count);
            ord_arr = arena_.allocate_array<std::uint64_t>(count);

            for (std::uint64_t i = 0; i < count; ++i) {
                arg_arr[i] = args[i].get();
                ord_arr[i] = (i < orders.size()) ? orders[i] : 0;
            }
        }

        NodeKey key = NodeKey::make_applied(func_id, count, arg_arr, ord_arr);

        return MathExpression(intern(key, [&] {
            auto* n = arena_.make<MathExpressionNode>();
            n->type = NodeType::AppliedFunction;
            n->applied.func_id   = func_id;
            n->applied.arg_count = count;
            n->applied.args      = arg_arr;
            n->applied.orders    = ord_arr;
            return n;
        }));
    }

    MathExpression applied(std::uint64_t func_id, std::uint64_t order, MathExpression arg) {
        return applied(func_id, std::vector<MathExpression>{ arg }, std::vector<std::uint64_t>{ order });
    }

    MathExpression pos_inf() { return typed_leaf(NodeType::PositiveInfinity); }
    MathExpression neg_inf() { return typed_leaf(NodeType::NegativeInfinity); }
    MathExpression nan_expr() { return typed_leaf(NodeType::NaN); }
    MathExpression undefined_expr() { return typed_leaf(NodeType::Undefined); }
    MathExpression indeterminate_expr() { return typed_leaf(NodeType::Indeterminate); }
    MathExpression invalid_expr() { return typed_leaf(NodeType::Invalid); }

    MathExpression variable_by_id(std::uint64_t id) {
        NodeKey key = NodeKey::make_variable(id);
        return MathExpression(intern(key, [&] {
            auto* n = arena_.make<MathExpressionNode>();
            n->type = NodeType::Variable;
            n->variable.var_id = id;
            return n;
        }));
    }

    MathExpression canonicalize(MathExpressionNode* n) {
        if (!n) return MathExpression{};
        switch (n->type) {
            case NodeType::Constant:          return constant(n->constant);
            case NodeType::Variable:          return variable_by_id(n->variable.var_id);
            case NodeType::PositiveInfinity:  return pos_inf();
            case NodeType::NegativeInfinity:  return neg_inf();
            case NodeType::NaN:               return nan_expr();
            case NodeType::Undefined:         return undefined_expr();
            case NodeType::Indeterminate:     return indeterminate_expr();
            case NodeType::Invalid:           return invalid_expr();
            case NodeType::Power: {
                auto b = canonicalize(n->power.base);
                auto e = canonicalize(n->power.exponent);
                return power(b, e);
            }
            case NodeType::AppliedFunction: {
                const std::uint64_t cnt = n->applied.arg_count;
                std::vector<MathExpression> args; args.reserve(cnt);
                std::vector<std::uint64_t>  ords; ords.reserve(cnt);

                for (std::uint64_t i = 0; i < cnt; ++i) {
                    args.push_back(canonicalize(n->applied.args[i]));
                    ords.push_back(n->applied.orders[i]);
                }

                return applied(n->applied.func_id, args, ords);
            }
            default:
                if (NodeKeyHash::is_unary(n->type)) {
                    auto child = canonicalize(n->unary.child);
                    return unary(n->type, child);
                }
                if (NodeKeyHash::is_binary(n->type)) {
                    auto l = canonicalize(n->binary.left);
                    auto r = canonicalize(n->binary.right);
                    return binary(n->type, l, r);
                }
                return invalid_expr();
        }
    }

public:
    const VariableTable& variables() const noexcept { return vars_;  }
          VariableTable& variables()       noexcept { return vars_;  }
    const VariableTable& functions() const noexcept { return funcs_; }
          VariableTable& functions()       noexcept { return funcs_; }

    std::uint64_t function_id(const std::string& name) {
        return funcs_.get_or_create(name);
    }

private:
    MathExpression typed_leaf(NodeType t) {
        NodeKey key = NodeKey::make_typed_leaf(t);
        return MathExpression(intern(key, [&] {
            auto* n = arena_.make<MathExpressionNode>();
            n->type = t;
            return n;
        }));
    }

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
