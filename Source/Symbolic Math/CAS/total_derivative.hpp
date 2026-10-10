#ifndef FIZMO_MATH_TOTAL_DERIVATIVE_HPP
#define FIZMO_MATH_TOTAL_DERIVATIVE_HPP

#include "../Common/main_convenience.hpp"

#include <string>
#include <vector>

namespace fizmo {
namespace math {
namespace cas {

class DependencyContext {
public:
    struct Dependent { std::string var; std::string indep; std::string prime; };

    void add(std::string var, std::string indep, std::string prime) {
        deps_.push_back({ std::move(var), std::move(indep), std::move(prime) });
    }

    void register_function(const std::string& base, const std::string& indep, unsigned max_order = 2) {
        std::string cur = base;

        for (unsigned k = 0; k < max_order; ++k) {
            std::string nxt = cur + "'";
            deps_.push_back({ cur, indep, nxt });
            cur = nxt;
        }
    }

    const std::vector<Dependent>& dependents() const { return deps_; }
private:
    std::vector<Dependent> deps_;
};

inline Expression total_derivative(SymbolicContext& ctx, const Expression& F, const std::string& indep, const DependencyContext& deps) {
    Expression result = DIFFERENTIATE(ctx, F, indep);

    for (const auto& d : deps.dependents()) {
        Expression term = MUL(ctx, DIFFERENTIATE(ctx, F, d.var), VARIABLE(ctx, d.prime));
        result = ADD(ctx, result, term);
    }

    return SIMPLIFY(ctx, result);
}

inline Expression total_derivative(SymbolicContext& ctx, const Expression& F, const std::string& indep, const DependencyContext& deps, unsigned order) {
    Expression r = F;
    for (unsigned k = 0; k < order; ++k) r = total_derivative(ctx, r, indep, deps);
    return r;
}

inline Expression total_derivative(const Expression& F, const std::string& indep, const DependencyContext& deps) {
    return total_derivative(global_context(), F, indep, deps);
}

inline Expression total_derivative(const Expression& F, const std::string& indep, const DependencyContext& deps, unsigned order) {
    return total_derivative(global_context(), F, indep, deps, order);
}

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_TOTAL_DERIVATIVE_HPP