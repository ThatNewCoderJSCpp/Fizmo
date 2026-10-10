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

    MathExpression substitute(MathExpression expr, const SubstitutionMap& smap);

    MathExpression substitute(MathExpression expr, const std::unordered_map<std::string, MathExpression>& named);

    MathExpression substitute(MathExpression expr, std::initializer_list<std::pair<std::string, MathExpression>> pairs);

    MathExpression substitute(MathExpression expr, const std::unordered_map<std::string, double>& named);

    MathExpression substitute(MathExpression expr, std::initializer_list<std::pair<std::string, double>> pairs);

    MathExpression substitute(MathExpression expr, std::initializer_list<MathExpression> positional);

    MathExpression substitute(MathExpression expr, std::initializer_list<double> positional);

    MathExpression substitute(MathExpression expr, const std::string& var_name, MathExpression repl) { return substitute(expr, {{var_name, repl}}); }
    MathExpression substitute(MathExpression expr, const std::string& var_name, double val) { return substitute(expr, {{var_name, val}}); }

    MathExpression substitute(MathExpression expr, MathExpression from, MathExpression to) {
        SubstitutionMap smap;
        smap.by_node[from.get()] = to.get();
        return substitute(expr, smap);
    }

    MathExpression substitute(MathExpression expr, std::initializer_list<std::pair<MathExpression, MathExpression>> pairs);

private:
    MathExpressionManager&    mgr_;
    VariableTable&             vars_;
    MathExpressionSimplifier&  simp_;
    std::unordered_map<MathExpressionNode*, MathExpressionNode*> cache_;

    MathExpression walk(MathExpressionNode* n, const SubstitutionMap& smap);

    MathExpression walk_inner(MathExpressionNode* n, const SubstitutionMap& smap);
};

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_SUBSTITUTER_CLASS_HPP