#ifndef FIZMO_MATH_INTEGRATION_ALGEBRAIC_RULES_HPP
#define FIZMO_MATH_INTEGRATION_ALGEBRAIC_RULES_HPP

#include "../integrator.hpp"
#include <cmath>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

bool extract_var_squared(MathExpressionNode* n, std::uint64_t vid, double& coeff);

struct QuadraticDecomp {
    double               x2_coeff;   
    MathExpressionNode*  free_node;  
    double               free_val;   
};

bool match_ax2_plus_b(MathExpressionNode* n, std::uint64_t vid, QuadraticDecomp& q);

MathExpression symbolic_sqrt_free(MathExpressionManager& mgr, MathExpressionNode* n);

int classify_quadratic(const QuadraticDecomp& q);

double quadratic_ratio(const QuadraticDecomp& q);

bool match_x_times_sqrt_quad(MathExpressionNode* n, std::uint64_t vid, MathExpressionNode*& sqrt_inner);

MathExpression try_algebraic_forms(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& simp,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
);

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATION_ALGEBRAIC_RULES_HPP