#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

auto MathExpressionSubstitutor::substitute(MathExpression expr, const SubstitutionMap& smap) -> MathExpression {
        cache_.clear();
        MathExpression result = walk(expr.get(), smap);
        cache_.clear();
        return simp_.simplify(result);
    }

auto MathExpressionSubstitutor::substitute(MathExpression expr, const std::unordered_map<std::string, MathExpression>& named) -> MathExpression {
        SubstitutionMap smap;
        for (auto& [name, repl] : named) {
            auto id = vars_.get(name);
            if (id != VariableTable::invalid_id) smap.by_var[id] = repl.get();
        }
        return substitute(expr, smap);
    }

auto MathExpressionSubstitutor::substitute(MathExpression expr, std::initializer_list<std::pair<std::string, MathExpression>> pairs) -> MathExpression {
        SubstitutionMap smap;
        for (auto& [name, repl] : pairs) {
            auto id = vars_.get(name);
            if (id != VariableTable::invalid_id) smap.by_var[id] = repl.get();
        }
        return substitute(expr, smap);
    }

auto MathExpressionSubstitutor::substitute(MathExpression expr, const std::unordered_map<std::string, double>& named) -> MathExpression {
        SubstitutionMap smap;
        for (auto& [name, val] : named) {
            auto id = vars_.get(name);
            if (id != VariableTable::invalid_id) smap.by_var[id] = mgr_.constant(val).get();
        }
        return substitute(expr, smap);
    }

auto MathExpressionSubstitutor::substitute(MathExpression expr, std::initializer_list<std::pair<std::string, double>> pairs) -> MathExpression {
        SubstitutionMap smap;
        for (auto& [name, val] : pairs) {
            auto id = vars_.get(name);
            if (id != VariableTable::invalid_id) smap.by_var[id] = mgr_.constant(val).get();
        }
        return substitute(expr, smap);
    }

auto MathExpressionSubstitutor::substitute(MathExpression expr, std::initializer_list<MathExpression> positional) -> MathExpression {
        SubstitutionMap smap;
        std::uint64_t id = 0;
        for (auto& repl : positional) {
            if (id >= vars_.size()) break;
            smap.by_var[id] = repl.get();
            ++id;
        }
        return substitute(expr, smap);
    }

auto MathExpressionSubstitutor::substitute(MathExpression expr, std::initializer_list<double> positional) -> MathExpression {
        SubstitutionMap smap;
        std::uint64_t id = 0;
        for (double val : positional) {
            if (id >= vars_.size()) break;
            smap.by_var[id] = mgr_.constant(val).get();
            ++id;
        }
        return substitute(expr, smap);
    }

auto MathExpressionSubstitutor::substitute(MathExpression expr, std::initializer_list<std::pair<MathExpression, MathExpression>> pairs) -> MathExpression {
        SubstitutionMap smap;
        for (auto& [from, to] : pairs) smap.by_node[from.get()] = to.get();
        return substitute(expr, smap);
    }

auto MathExpressionSubstitutor::walk(MathExpressionNode* n, const SubstitutionMap& smap) -> MathExpression {
        if (!n) return MathExpression{};

        {
            auto it = smap.by_node.find(n);
            if (it != smap.by_node.end()) return MathExpression(it->second);
        }

        {
            auto it = cache_.find(n);
            if (it != cache_.end()) return MathExpression(it->second);
        }

        MathExpression result = walk_inner(n, smap);
        cache_[n] = result.get();
        return result;
    }

auto MathExpressionSubstitutor::walk_inner(MathExpressionNode* n, const SubstitutionMap& smap) -> MathExpression {
        switch (n->type) {

        case NodeType::Constant:
        case NodeType::PositiveInfinity:
        case NodeType::NegativeInfinity:
        case NodeType::NaN:
        case NodeType::Undefined:
        case NodeType::Indeterminate:
            return MathExpression(n);   

        case NodeType::Variable: {
            auto it = smap.by_var.find(n->variable.var_id);
            if (it != smap.by_var.end()) return MathExpression(it->second);
            return MathExpression(n);  
        }
        
        case NodeType::Power: {
            MathExpression base = walk(n->power.base,     smap);
            MathExpression exp  = walk(n->power.exponent, smap);
            return mgr_.power(base, exp);
        }

        case NodeType::AppliedFunction: {
            const std::uint64_t cnt = n->applied.arg_count;
            std::vector<MathExpression> args; args.reserve(cnt);
            std::vector<std::uint64_t>  ords; ords.reserve(cnt);

            for (std::uint64_t i = 0; i < cnt; ++i) {
                args.push_back(walk(n->applied.args[i], smap));
                ords.push_back(n->applied.orders[i]);
            }
            
            return mgr_.applied(n->applied.func_id, args, ords);
        }

        default: {
            if (NodeKeyHash::is_unary(n->type)) {
                MathExpression child = walk(n->unary.child, smap);
                return mgr_.unary(n->type, child);
            }
            if (NodeKeyHash::is_binary(n->type)) {
                MathExpression l = walk(n->binary.left,  smap);
                MathExpression r = walk(n->binary.right, smap);
                return mgr_.binary(n->type, l, r);
            }
            // Unknown / future node type 
            return MathExpression(n);
        }
        } 
    }

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
