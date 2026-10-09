#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {
namespace complex_symbols {

auto ComplexVariableTable::get_or_create(const std::string& name) -> std::uint64_t {
        auto it = name_to_id_.find(name);
        if (it != name_to_id_.end()) return it->second;
        std::uint64_t id = static_cast<std::uint64_t>(names_.size());
        names_.push_back(name);
        name_to_id_.emplace(name, id);
        return id;
    }

auto ComplexNodeKey::make_constant(double r, double i) -> ComplexNodeKey {
        ComplexNodeKey k{};
        k.type = ComplexNodeType::Constant;
        k.constant.real = r;
        k.constant.imag = i;
        return k;
    }

auto ComplexNodeKey::make_power(ComplexMathExpressionNode* b, ComplexMathExpressionNode* e) -> ComplexNodeKey {
        ComplexNodeKey k{};
        k.type = ComplexNodeType::Power;
        k.power.base = b;
        k.power.exponent = e;
        return k;
    }

auto ComplexNodeKeyHash::operator()(ComplexNodeKey const& k) const noexcept -> std::size_t {
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

auto ComplexNodeKeyHash::is_typed_leaf(ComplexNodeType t) -> bool {
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

auto ComplexNodeKeyHash::is_binary(ComplexNodeType t) -> bool {
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

auto ComplexNodeKeyHash::is_unary(ComplexNodeType t) -> bool {
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

auto ComplexNodeKeyEq::operator()(ComplexNodeKey const& a, ComplexNodeKey const& b) const noexcept -> bool {
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

auto ComplexMathExpressionManager::constant(double r, double i) -> ComplexMathExpression {
        ComplexNodeKey key = ComplexNodeKey::make_constant(r, i);

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = ComplexNodeType::Constant;
            n->constant.real = r;
            n->constant.imag = i;
            return n;
        }));
    }

auto ComplexMathExpressionManager::variable(const std::string& name) -> ComplexMathExpression {
        std::uint64_t id = vars_.get_or_create(name);
        ComplexNodeKey key = ComplexNodeKey::make_variable(id);

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = ComplexNodeType::Variable;
            n->variable.var_id = id;
            return n;
        }));
    }

auto ComplexMathExpressionManager::variable_by_id(std::uint64_t id) -> ComplexMathExpression {
        ComplexNodeKey key = ComplexNodeKey::make_variable(id);

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = ComplexNodeType::Variable;
            n->variable.var_id = id;
            return n;
        }));
    }

auto ComplexMathExpressionManager::unary(ComplexNodeType t, ComplexMathExpression a) -> ComplexMathExpression {
        ComplexNodeKey key = ComplexNodeKey::make_unary(t, a.get());

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = t;
            n->unary.child = a.get();
            return n;
        }));
    }

auto ComplexMathExpressionManager::binary(ComplexNodeType t, ComplexMathExpression a, ComplexMathExpression b) -> ComplexMathExpression {
        ComplexNodeKey key = ComplexNodeKey::make_binary(t, a.get(), b.get());

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = t;
            n->binary.left = a.get();
            n->binary.right = b.get();
            return n;
        }));
    }

auto ComplexMathExpressionManager::power(ComplexMathExpression base, ComplexMathExpression exp) -> ComplexMathExpression {
        ComplexNodeKey key = ComplexNodeKey::make_power(base.get(), exp.get());

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = ComplexNodeType::Power;
            n->power.base = base.get();
            n->power.exponent = exp.get();
            return n;
        }));
    }

auto ComplexMathExpressionManager::canonicalize(ComplexMathExpressionNode* n) -> ComplexMathExpression {
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

auto ComplexMathExpressionManager::typed_leaf(ComplexNodeType t) -> ComplexMathExpression {
        ComplexNodeKey key = ComplexNodeKey::make_typed_leaf(t);

        return ComplexMathExpression(intern(key, [&] {
            auto* n = arena_.make<ComplexMathExpressionNode>();
            n->type = t;
            return n;
        }));
    }

} // namespace complex_symbols
} // namespace cas
} // namespace math
} // namespace fizmo
