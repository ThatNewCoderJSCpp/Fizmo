#include "newton.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace numerics {

namespace {

double max_norm(const std::vector<double>& v) {
    double m = 0.0;
    for (double a : v) {
        if (!std::isfinite(a)) return INFINITY;
        m = std::max(m, std::fabs(a));
    }
    return m;
}

} // namespace

SparseNewtonResult newton_sparse(const SparseResidual& residual, const SparseJacobian& jacobian, std::vector<double>& x, const SparseNewtonOptions& options) {
    SparseNewtonResult out;
    SparseSolver solver;
    solver.method = options.method;
    std::vector<double> r;
    residual(x, r);
    double rn = max_norm(r);
    CsrMatrix J;
    bool have = false;
    for (int it = 0; it < options.max_iterations; ++it) {
        out.iterations = it;
        out.residual = rn;
        if (!std::isfinite(rn)) {
            out.message = "the residual is not finite";
            return out;
        }
        if (rn <= options.tolerance) {
            out.converged = true;
            break;
        }
        if (options.cancelled && options.cancelled()) {
            out.message = "cancelled";
            return out;
        }
        if (!have || !options.reuse_jacobian) {
            jacobian(x, J);
            if (!solver.factor(J)) {
                out.message = "the Jacobian is singular";
                return out;
            }
            have = true;
        }
        std::vector<double> step(r.size());
        for (std::size_t i = 0; i < r.size(); ++i) step[i] = -r[i];
        if (!solver.solve(step)) {
            out.message = "the linear solver did not converge";
            return out;
        }
        double lambda = 1.0;
        std::vector<double> trial(x.size()), rt;
        double tn = rn;
        for (int tries = 0; tries < (options.line_search ? 12 : 1); ++tries) {
            for (std::size_t i = 0; i < x.size(); ++i) trial[i] = x[i] + lambda * step[i];
            residual(trial, rt);
            tn = max_norm(rt);
            if (!options.line_search || tn < rn * (1.0 - 1e-4 * lambda)) break;
            lambda *= 0.5;
        }
        x = trial;
        r = rt;
        rn = tn;
        out.iterations = it + 1;
        out.residual = rn;
        if (max_norm(step) * lambda <= options.step_tolerance * (1.0 + max_norm(x))) {
            out.converged = rn <= std::max(options.tolerance, 1e-8);
            break;
        }
    }
    if (!out.converged && rn <= options.tolerance) out.converged = true;
    out.linear_iterations = solver.iterations();
    out.solver = solver.name();
    if (!out.converged && out.message.empty()) out.message = "Newton's method did not converge";
    return out;
}

} // namespace numerics
} // namespace fizmo
