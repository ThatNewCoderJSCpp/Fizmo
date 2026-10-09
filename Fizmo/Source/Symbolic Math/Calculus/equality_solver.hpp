#ifndef FIZMO_EQUATION_SOLVER_HPP
#define FIZMO_EQUATION_SOLVER_HPP

#include "../CAS/expression_wrapper.hpp"
#include "../CAS/symbolic_context.hpp"
#include "../../Basic/constants.hpp"
#include "../Common/main_convenience.hpp"
#include "../Common/intervals.hpp"
#include "../Common/points.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <functional>
#include <utility>

namespace fizmo {
namespace math {
namespace solvers {

struct SolverResult {
    double        value;
    bool          converged;
    double        residual;
    std::uint64_t iterations;
};

enum class CriticalPointType {
    Min = 0,
    Max,
    Inflection,
    Saddle,
    Degenerate,
    Inconclusive
};

std::ostream& operator<<(std::ostream& os, CriticalPointType type);

struct CriticalPoint1D {
    double             point;
    CriticalPointType  type;
    double             function_value;
    double             residual;
    bool               converged;
};

struct SolverOptions {
    double        tolerance              = constants::middle_epsilon();
    std::uint64_t max_iterations         = 1ULL << 7;
    std::uint64_t coarse_scan_points     = 1ULL << 10;
    std::uint64_t adaptive_refine_depth  = 1ULL << 3;
    double        adaptive_growth_factor = 8.0;
};

class EquationSolver {
public:
    static std::vector<SolverResult> find_all(
        cas::SymbolicContext& ctx,
        const cas::Expression& expr, double target,
        const std::string& var, const Interval& domain,
        const SolverOptions& opts = {}
    ) {
        auto [f, df] = make_lambdas(ctx, expr, target, var);
        return find_all_impl(f, df, domain, opts);
    }

    static std::vector<SolverResult> find_all(
        cas::SymbolicContext& ctx,
        const cas::Expression& expr, double target,
        const std::string& var, double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_all(ctx, expr, target, var, make_domain(lo, hi), opts); }

    static std::vector<SolverResult> find_all(
        cas::SymbolicContext& ctx,
        const cas::Expression& lhs, const cas::Expression& rhs,
        const std::string& var, const Interval& domain,
        const SolverOptions& opts = {}
    ) { return find_all(ctx, lhs - rhs, 0.0, var, domain, opts); }

    static std::vector<SolverResult> find_all(
        cas::SymbolicContext& ctx,
        const cas::Expression& lhs, const cas::Expression& rhs,
        const std::string& var, double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_all(ctx, lhs - rhs, 0.0, var, make_domain(lo, hi), opts); }

    static SolverResult find_nearest(
        cas::SymbolicContext& ctx,
        const cas::Expression& expr, double target,
        const std::string& var, double guess,
        const SolverOptions& opts = {}
    ) {
        auto [f, df] = make_lambdas(ctx, expr, target, var);
        return find_nearest_impl(f, df, guess, opts);
    }

    static SolverResult find_nearest(
        cas::SymbolicContext& ctx,
        const cas::Expression& lhs, const cas::Expression& rhs,
        const std::string& var, double guess,
        const SolverOptions& opts = {}
    ) { return find_nearest(ctx, lhs - rhs, 0.0, var, guess, opts); }

    static std::vector<SolverResult> find_all(
        const cas::Expression& expr, double target, const std::string& var,
        const Interval& domain, const SolverOptions& opts = {}
    ) { return find_all(cas::global_context(), expr, target, var, domain, opts); }

    static std::vector<SolverResult> find_all(
        const cas::Expression& expr, double target, const std::string& var,
        double lo, double hi, const SolverOptions& opts = {}
    ) { return find_all(cas::global_context(), expr, target, var, lo, hi, opts); }

    static std::vector<SolverResult> find_all(
        const cas::Expression& lhs, const cas::Expression& rhs, const std::string& var,
        const Interval& domain, const SolverOptions& opts = {}
    ) { return find_all(cas::global_context(), lhs, rhs, var, domain, opts); }

    static std::vector<SolverResult> find_all(
        const cas::Expression& lhs, const cas::Expression& rhs, const std::string& var,
        double lo, double hi, const SolverOptions& opts = {}
    ) { return find_all(cas::global_context(), lhs, rhs, var, lo, hi, opts); }

    static SolverResult find_nearest(
        const cas::Expression& expr, double target, const std::string& var,
        double guess, const SolverOptions& opts = {}
    ) { return find_nearest(cas::global_context(), expr, target, var, guess, opts); }

    static SolverResult find_nearest(
        const cas::Expression& lhs, const cas::Expression& rhs, const std::string& var,
        double guess, const SolverOptions& opts = {}
    ) { return find_nearest(cas::global_context(), lhs, rhs, var, guess, opts); }

    static std::vector<SolverResult> find_roots(
        cas::SymbolicContext& ctx, const cas::Expression& expr,
        const std::string& var, const Interval& domain,
        const SolverOptions& opts = {}
    ) { return find_all(ctx, expr, 0.0, var, domain, opts); }

    static std::vector<SolverResult> find_roots(
        cas::SymbolicContext& ctx, const cas::Expression& expr,
        const std::string& var, double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_all(ctx, expr, 0.0, var, lo, hi, opts); }

    static std::vector<SolverResult> find_roots(
        const cas::Expression& expr, const std::string& var,
        const Interval& domain, const SolverOptions& opts = {}
    ) { return find_all(cas::global_context(), expr, 0.0, var, domain, opts); }

    static std::vector<SolverResult> find_roots(
        const cas::Expression& expr, const std::string& var,
        double lo, double hi, const SolverOptions& opts = {}
    ) { return find_all(cas::global_context(), expr, 0.0, var, lo, hi, opts); }

    static SolverResult nearest_root(
        cas::SymbolicContext& ctx,
        const cas::Expression& expr,
        const std::string& var, double guess,
        const SolverOptions& opts = {}
    ) { return find_nearest(ctx, expr, 0.0, var, guess, opts); }

    static SolverResult nearest_root(
        const cas::Expression& expr,
        const std::string& var, double guess,
        const SolverOptions& opts = {}
    ) { return nearest_root(cas::global_context(), expr, var, guess, opts); }

    static std::vector<SolverResult> inverse(
        cas::SymbolicContext& ctx, const cas::Expression& expr,
        double y, const std::string& var, const Interval& domain,
        const SolverOptions& opts = {}
    ) { return find_all(ctx, expr, y, var, domain, opts); }

    static std::vector<SolverResult> inverse(
        cas::SymbolicContext& ctx, const cas::Expression& expr,
        double y, const std::string& var, double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_all(ctx, expr, y, var, lo, hi, opts); }

    static std::vector<SolverResult> inverse(
        const cas::Expression& expr, double y, const std::string& var,
        const Interval& domain, const SolverOptions& opts = {}
    ) { return find_all(cas::global_context(), expr, y, var, domain, opts); }

    static std::vector<SolverResult> inverse(
        const cas::Expression& expr, double y, const std::string& var,
        double lo, double hi, const SolverOptions& opts = {}
    ) { return find_all(cas::global_context(), expr, y, var, lo, hi, opts); }

    static SolverResult inverse_nearest(
        cas::SymbolicContext& ctx, const cas::Expression& expr,
        double y, const std::string& var, double guess,
        const SolverOptions& opts = {}
    ) { return find_nearest(ctx, expr, y, var, guess, opts); }

    static SolverResult inverse_nearest(
        const cas::Expression& expr, double y, const std::string& var,
        double guess, const SolverOptions& opts = {}
    ) { return find_nearest(cas::global_context(), expr, y, var, guess, opts); }

    static std::vector<CriticalPoint1D> find_critical_points(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var,
        const Interval& domain,
        const SolverOptions& opts = {}
    );

    static std::vector<CriticalPoint1D> find_critical_points(
        const cas::Expression& f_expr,
        const std::string& var,
        const Interval& domain,
        const SolverOptions& opts = {}
    ) { return find_critical_points(cas::global_context(), f_expr, var, domain, opts); }

    static std::vector<CriticalPoint1D> find_critical_points(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var,
        double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_critical_points(ctx, f_expr, var, make_domain(lo, hi), opts); }

    static std::vector<CriticalPoint1D> find_critical_points(
        const cas::Expression& f_expr,
        const std::string& var,
        double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_critical_points(cas::global_context(), f_expr, var, lo, hi, opts); }

    static std::vector<CriticalPoint1D> find_local_minima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& var, const Interval& domain,
        const SolverOptions& opts = {}
    );

    static std::vector<CriticalPoint1D> find_local_minima(
        const cas::Expression& f, const std::string& var,
        const Interval& domain, const SolverOptions& opts = {}
    ) { return find_local_minima(cas::global_context(), f, var, domain, opts); }

    static std::vector<CriticalPoint1D> find_local_minima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& var, double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_local_minima(ctx, f, var, make_domain(lo, hi), opts); }

    static std::vector<CriticalPoint1D> find_local_minima(
        const cas::Expression& f, const std::string& var,
        double lo, double hi, const SolverOptions& opts = {}
    ) { return find_local_minima(cas::global_context(), f, var, lo, hi, opts); }

    static std::vector<CriticalPoint1D> find_local_maxima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& var, const Interval& domain,
        const SolverOptions& opts = {}
    );

    static std::vector<CriticalPoint1D> find_local_maxima(
        const cas::Expression& f, const std::string& var,
        const Interval& domain, const SolverOptions& opts = {}
    ) { return find_local_maxima(cas::global_context(), f, var, domain, opts); }

    static std::vector<CriticalPoint1D> find_local_maxima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& var, double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_local_maxima(ctx, f, var, make_domain(lo, hi), opts); }

    static std::vector<CriticalPoint1D> find_local_maxima(
        const cas::Expression& f, const std::string& var,
        double lo, double hi, const SolverOptions& opts = {}
    ) { return find_local_maxima(cas::global_context(), f, var, lo, hi, opts); }

    static std::vector<CriticalPoint1D> find_inflection_points(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var,
        const Interval& domain,
        const SolverOptions& opts = {}
    );

    static std::vector<CriticalPoint1D> find_inflection_points(
        const cas::Expression& f_expr,
        const std::string& var,
        const Interval& domain,
        const SolverOptions& opts = {}
    ) { return find_inflection_points(cas::global_context(), f_expr, var, domain, opts); }

    static std::vector<CriticalPoint1D> find_inflection_points(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var,
        double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_inflection_points(ctx, f_expr, var, make_domain(lo, hi), opts); }

    static std::vector<CriticalPoint1D> find_inflection_points(
        const cas::Expression& f_expr,
        const std::string& var,
        double lo, double hi,
        const SolverOptions& opts = {}
    ) { return find_inflection_points(cas::global_context(), f_expr, var, lo, hi, opts); }

private:
    using Fn = std::function<double(double)>;
    static bool is_valid(double x) noexcept { return std::isfinite(x); }
    static bool sign_change(double a, double b) noexcept { return is_valid(a) && is_valid(b) && ((a > 0.0) != (b > 0.0)); }
    static bool is_root(double fv, double tol) noexcept { return is_valid(fv) && std::abs(fv) <= tol; }
    static double clamp_tol(double t) noexcept { return t > constants::type_epsilon() ? t : constants::type_epsilon(); }
    static Interval make_domain(double lo, double hi) noexcept;
    static double finite_lo(const Interval& dom) noexcept { return dom.lower.is_negative_infinity() ? -1e300 : dom.lower.value; }
    static double finite_hi(const Interval& dom) noexcept { return dom.upper.is_positive_infinity() ?  1e300 : dom.upper.value; }
    struct LambdaPair { Fn f; Fn df; };
    static LambdaPair make_lambdas(
        cas::SymbolicContext& ctx,
        const cas::Expression& expr,
        double target,
        const std::string& var
    );
    static void adaptive_scan(
        const Fn& f,
        double xa, double fa,
        double xb, double fb,
        std::uint64_t depth,
        double growth_threshold,
        double tol,
        std::vector<Interval>& out
    );

    static std::vector<Interval> scan_intervals(
        const Fn& f,
        const Interval& domain,
        const SolverOptions& opts
    );
    static SolverResult brent(
        const Fn& f,
        double a, double b,
        double tol,
        std::uint64_t max_iter
    );
    static SolverResult halley_newton(
        const Fn& f,
        const Fn& df,
        double x0,
        double tol,
        std::uint64_t max_iter
    );
    static SolverResult hybrid_solve(
        const Fn& f, const Fn& df,
        double a, double b,
        double tol, std::uint64_t max_iter
    );
    static std::vector<double> extrema_near_zero(
        const Fn& f, const Fn& df,
        const Interval& domain,
        std::uint64_t N, double tol
    );
    static bool is_duplicate(
        const std::vector<SolverResult>& results,
        double val, double tol
    ) {
        for (auto& r : results) if (std::abs(r.value - val) < tol * 10.0) return true;
        return false;
    }
    static void try_add(
        std::vector<SolverResult>& results,
        const SolverResult& r,
        const Interval& domain,
        double tol
    );
    static std::vector<SolverResult> find_all_impl(
        const Fn& f, const Fn& df,
        const Interval& domain,
        const SolverOptions& opts
    );
    static SolverResult find_nearest_impl(
        const Fn& f, const Fn& df,
        double guess,
        const SolverOptions& opts
    );
    static std::vector<CriticalPoint1D> classify_critical_points_1d(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const cas::Expression& fpp_expr,
        const std::string& var,
        const std::vector<SolverResult>& solutions
    );
    static std::vector<CriticalPoint1D> classify_inflection_points(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const cas::Expression& fppp_expr,
        const std::string& var,
        const std::vector<SolverResult>& solutions
    );
    static std::vector<CriticalPoint1D> filter_by_type(std::vector<CriticalPoint1D> pts, CriticalPointType type);
};

} // namespace solvers
} // namespace math
} // namespace fizmo

#endif // FIZMO_EQUATION_SOLVER_HPP