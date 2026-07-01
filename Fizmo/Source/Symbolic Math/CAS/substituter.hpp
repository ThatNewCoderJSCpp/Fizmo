#ifndef FIZMO_MATH_SUBSTITUTER_CLASS_HPP
#define FIZMO_MATH_SUBSTITUTER_CLASS_HPP

#include "expression.hpp"
#include "simplifier.hpp"

#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

struct SubstitutionMap {
    std::unordered_map<std::uint64_t, MathExpressionNode*> by_var;
    std::unordered_map<MathExpressionNode*, MathExpressionNode*> by_node;
};

class MathExpressionSubstitutor {
public:
    MathExpressionSubstitutor(MathExpressionManager& mgr, VariableTable& vars, MathExpressionSimplifier& simp) : mgr_(mgr), vars_(vars), simp_(simp) {}

    MathExpression substitute(MathExpression expr, const SubstitutionMap& smap) {
        cache_.clear();
        MathExpression result = walk(expr.get(), smap);
        cache_.clear();
        return simp_.simplify(result);
    }

    MathExpression substitute(MathExpression expr, const std::unordered_map<std::string, MathExpression>& named) {
        SubstitutionMap smap;
        for (auto& [name, repl] : named) {
            auto id = vars_.get(name);
            if (id != VariableTable::invalid_id) smap.by_var[id] = repl.get();
        }
        return substitute(expr, smap);
    }

    MathExpression substitute(MathExpression expr, std::initializer_list<std::pair<std::string, MathExpression>> pairs) {
        SubstitutionMap smap;
        for (auto& [name, repl] : pairs) {
            auto id = vars_.get(name);
            if (id != VariableTable::invalid_id) smap.by_var[id] = repl.get();
        }
        return substitute(expr, smap);
    }

    MathExpression substitute(MathExpression expr, const std::unordered_map<std::string, double>& named) {
        SubstitutionMap smap;
        for (auto& [name, val] : named) {
            auto id = vars_.get(name);
            if (id != VariableTable::invalid_id) smap.by_var[id] = mgr_.constant(val).get();
        }
        return substitute(expr, smap);
    }

    MathExpression substitute(MathExpression expr, std::initializer_list<std::pair<std::string, double>> pairs) {
        SubstitutionMap smap;
        for (auto& [name, val] : pairs) {
            auto id = vars_.get(name);
            if (id != VariableTable::invalid_id) smap.by_var[id] = mgr_.constant(val).get();
        }
        return substitute(expr, smap);
    }

    MathExpression substitute(MathExpression expr, std::initializer_list<MathExpression> positional) {
        SubstitutionMap smap;
        std::uint64_t id = 0;
        for (auto& repl : positional) {
            if (id >= vars_.size()) break;
            smap.by_var[id] = repl.get();
            ++id;
        }
        return substitute(expr, smap);
    }

    MathExpression substitute(MathExpression expr, std::initializer_list<double> positional) {
        SubstitutionMap smap;
        std::uint64_t id = 0;
        for (double val : positional) {
            if (id >= vars_.size()) break;
            smap.by_var[id] = mgr_.constant(val).get();
            ++id;
        }
        return substitute(expr, smap);
    }

    MathExpression substitute(MathExpression expr, const std::string& var_name, MathExpression repl) { return substitute(expr, {{var_name, repl}}); }
    MathExpression substitute(MathExpression expr, const std::string& var_name, double val) { return substitute(expr, {{var_name, val}}); }

    MathExpression substitute(MathExpression expr, MathExpression from, MathExpression to) {
        SubstitutionMap smap;
        smap.by_node[from.get()] = to.get();
        return substitute(expr, smap);
    }

    MathExpression substitute(MathExpression expr, std::initializer_list<std::pair<MathExpression, MathExpression>> pairs) {
        SubstitutionMap smap;
        for (auto& [from, to] : pairs) smap.by_node[from.get()] = to.get();
        return substitute(expr, smap);
    }

private:
    MathExpressionManager&    mgr_;
    VariableTable&             vars_;
    MathExpressionSimplifier&  simp_;
    std::unordered_map<MathExpressionNode*, MathExpressionNode*> cache_;

    MathExpression walk(MathExpressionNode* n, const SubstitutionMap& smap) {
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

    MathExpression walk_inner(MathExpressionNode* n, const SubstitutionMap& smap) {
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
};

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_SUBSTITUTER_CLASS_HPP