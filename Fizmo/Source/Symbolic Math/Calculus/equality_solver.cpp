#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace solvers {

std::ostream& operator<<(std::ostream& os, CriticalPointType type) {
    switch (type) {
        case CriticalPointType::Min: os << "min"; break;
        case CriticalPointType::Max: os << "max"; break;
        case CriticalPointType::Inflection: os << "infelction"; break;
        case CriticalPointType::Saddle: os << "saddle"; break;
        case CriticalPointType::Degenerate: os << "degenerate"; break;
        case CriticalPointType::Inconclusive: os << "inconclusive"; break; 
    }
    return os;
}

auto EquationSolver::find_critical_points(
    cas::SymbolicContext& ctx,
    const cas::Expression& f_expr,
    const std::string& var,
    const Interval& domain,
    const SolverOptions& opts 
) -> std::vector<CriticalPoint1D> {
    cas::Expression fp  = ctx.derivative(f_expr, var);
    cas::Expression fpp = ctx.derivative(fp, var);
    auto solutions = find_roots(ctx, fp, var, domain, opts);
    return classify_critical_points_1d(ctx, f_expr, fpp, var, solutions);
}

auto EquationSolver::find_local_minima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& var, const Interval& domain,
        const SolverOptions& opts 
) -> std::vector<CriticalPoint1D> { return filter_by_type(find_critical_points(ctx, f, var, domain, opts), CriticalPointType::Min); }

auto EquationSolver::find_local_maxima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& var, const Interval& domain,
        const SolverOptions& opts 
) -> std::vector<CriticalPoint1D> { return filter_by_type(find_critical_points(ctx, f, var, domain, opts), CriticalPointType::Max); }

auto EquationSolver::find_inflection_points(
    cas::SymbolicContext& ctx,
    const cas::Expression& f_expr,
    const std::string& var,
    const Interval& domain,
    const SolverOptions& opts 
) -> std::vector<CriticalPoint1D> {
    cas::Expression fp   = ctx.derivative(f_expr, var);
    cas::Expression fpp  = ctx.derivative(fp, var);
    cas::Expression fppp = ctx.derivative(fpp, var);
    auto solutions = find_roots(ctx, fpp, var, domain, opts);
    return classify_inflection_points(ctx, f_expr, fppp, var, solutions);
}

auto EquationSolver::make_domain(double lo, double hi) noexcept -> Interval {
    if (lo > hi) std::swap(lo, hi);
    Endpoint ep_lo = std::isinf(lo) ? Endpoint::negative_infinity() : Endpoint::closed(lo);
    Endpoint ep_hi = std::isinf(hi) ? Endpoint::positive_infinity() : Endpoint::closed(hi);
    return {ep_lo, ep_hi};
}

auto EquationSolver::make_lambdas(
    cas::SymbolicContext& ctx,
    const cas::Expression& expr,
    double target,
    const std::string& var
) -> LambdaPair {
    cas::Expression equation = expr - ctx.constant(target);
    cas::Expression deriv    = ctx.derivative(equation, var);
    Fn f = [equation, var](double x) -> double {
        return equation.evaluate({{var, x}});
    };
    Fn df = [deriv, var](double x) -> double {
        return deriv.evaluate({{var, x}});
    };
    return {std::move(f), std::move(df)};
}

void EquationSolver::adaptive_scan(
    const Fn& f,
    double xa, double fa,
    double xb, double fb,
    std::uint64_t depth,
    double growth_threshold,
    double tol,
    std::vector<Interval>& out
) {
    if (sign_change(fa, fb)) {
        out.push_back({Endpoint::closed(xa), Endpoint::closed(xb)});
        return;
    }
    if (is_root(fa, tol)) {
        double half = (xb - xa) * 0.25;
        out.push_back({Endpoint::closed(xa - half), Endpoint::closed(xa + half)});
        return;
    }
    if (depth == 0) return;
    bool needs_refine = false;
    if (is_valid(fa) && is_valid(fb)) {
        double afa = std::abs(fa), afb = std::abs(fb);
        double lo_mag = std::min(afa, afb);
        double hi_mag = std::max(afa, afb);
        if (lo_mag > 0.0 && hi_mag / lo_mag > growth_threshold) needs_refine = true;
        if (lo_mag < tol * 1e6 && hi_mag > 1.0) needs_refine = true;
    } else {
        needs_refine = true;
    }

    if (!needs_refine) return;
    double xm = (xa + xb) * 0.5;
    double fm = f(xm);
    adaptive_scan(f, xa, fa, xm, fm, depth - 1, growth_threshold, tol, out);
    adaptive_scan(f, xm, fm, xb, fb, depth - 1, growth_threshold, tol, out);
}

auto EquationSolver::scan_intervals(
    const Fn& f,
    const Interval& domain,
    const SolverOptions& opts
) -> std::vector<Interval> {
    double tol = clamp_tol(opts.tolerance);
    double lo  = finite_lo(domain);
    double hi  = finite_hi(domain);
    std::uint64_t N = opts.coarse_scan_points;
    double step = (hi - lo) / static_cast<double>(N);
    std::vector<Interval> intervals;
    intervals.reserve(N / 8);
    double px = lo, pf = f(lo);
    for (std::uint64_t i = 1; i <= N; ++i) {
        double cx = (i == N) ? hi : lo + static_cast<double>(i) * step;
        double cf = f(cx);
        if (sign_change(pf, cf)) {
            adaptive_scan(f, px, pf, cx, cf, 1, opts.adaptive_growth_factor, tol, intervals);
        } else {
            adaptive_scan(f, px, pf, cx, cf, opts.adaptive_refine_depth, opts.adaptive_growth_factor, tol, intervals);
        }
        px = cx; pf = cf;
    }
    if (is_root(pf, tol)) {
        double half = step * 0.25;
        intervals.push_back({Endpoint::closed(hi - half), Endpoint::closed(hi)});
    }
    return intervals;
}

auto EquationSolver::brent(
    const Fn& f,
    double a, double b,
    double tol,
    std::uint64_t max_iter
) -> SolverResult {
    double fa = f(a), fb = f(b);
    if (!sign_change(fa, fb)) {
        double best = std::abs(fa) < std::abs(fb) ? a : b;
        double fbest = f(best);
        return {best, is_root(fbest, tol * 100), std::abs(fbest), 0};
    }
    if (std::abs(fa) < std::abs(fb)) {
        std::swap(a, b); std::swap(fa, fb);
    }
    double c = a, fc = fa;
    bool mflag = true;
    double d = 0.0, s = 0.0;
    for (std::uint64_t it = 0; it < max_iter; ++it) {
        if (std::abs(fb) <= tol) return {b, true, std::abs(fb), it};
        if (std::abs(b - a) < tol) {
            double r = (a + b) * 0.5;
            double fr = f(r);
            return {r, true, std::abs(fr), it};
        }
        if (std::abs(fa - fc) > tol && std::abs(fb - fc) > tol) {
            s = a*fb*fc / ((fa-fb)*(fa-fc))
              + b*fa*fc / ((fb-fa)*(fb-fc))
              + c*fa*fb / ((fc-fa)*(fc-fb));
        } else {
            s = b - fb * (b - a) / (fb - fa);
        }
        double lo3ab = (3*a + b) / 4.0, hiab = b;
        if (lo3ab > hiab) std::swap(lo3ab, hiab);
        bool reject =
            (s < lo3ab || s > hiab) ||
            ( mflag && std::abs(s-b) >= std::abs(b-c)*0.5) ||
            (!mflag && std::abs(s-b) >= std::abs(c-d)*0.5) ||
            ( mflag && std::abs(b-c) < tol) ||
            (!mflag && std::abs(c-d) < tol);
        if (reject) { s = (a + b) * 0.5; mflag = true; }
        else         { mflag = false; }
        double fs = f(s);
        if (!is_valid(fs)) {
            s = (a + b) * 0.5;
            fs = f(s);
            if (!is_valid(fs)) return {s, false, std::numeric_limits<double>::infinity(), it};
        }
        d = c; c = b; fc = fb;
        if (sign_change(fa, fs)) { b = s; fb = fs; }
        else                     { a = s; fa = fs; }
        if (std::abs(fa) < std::abs(fb)) { std::swap(a, b); std::swap(fa, fb); }
    }
    double r = (a + b) * 0.5;
    double fr = f(r);
    return {r, std::abs(fr) <= tol * 10.0, std::abs(fr), max_iter};
}

auto EquationSolver::halley_newton(
    const Fn& f,
    const Fn& df,
    double x0,
    double tol,
    std::uint64_t max_iter
) -> SolverResult {
    double x  = x0;
    double fx = f(x);
    if (!is_valid(fx)) return {x, false, std::numeric_limits<double>::infinity(), 0};
    for (std::uint64_t it = 0; it < max_iter; ++it) {
        if (is_root(fx, tol)) return {x, true, std::abs(fx), it};
        double dfx = df(x);
        if (!is_valid(dfx) || std::abs(dfx) < constants::type_epsilon()) {
            double h  = std::max(std::abs(x) * 1e-8, 1e-10);
            double fp = f(x + h), fm = f(x - h);
            if (is_valid(fp) && is_valid(fm))
                dfx = (fp - fm) / (2.0 * h);
            else
                return {x, is_root(fx, tol * 10), std::abs(fx), it};
        }
        if (std::abs(dfx) <= constants::type_epsilon()) return {x, is_root(fx, tol * 10), std::abs(fx), it};
        double h2  = std::max(std::abs(x) * 1e-6, 1e-8);
        double dfp = df(x + h2), dfm = df(x - h2);
        double d2fx = 0.0;
        bool use_halley = false;
        if (is_valid(dfp) && is_valid(dfm)) {
            d2fx = (dfp - dfm) / (2.0 * h2);
            use_halley = true;
        }
        double dx;
        if (use_halley) {
            double num   = 2.0 * fx * dfx;
            double denom = 2.0 * dfx * dfx - fx * d2fx;
            dx = (std::abs(denom) > constants::type_epsilon()) ? num / denom : fx / dfx;
        } else {
            dx = fx / dfx;
        }
        double xn = x - dx;
        if (!is_valid(xn)) return {x, is_root(fx, tol * 10), std::abs(fx), it};
        if (std::abs(dx) < tol * (1.0 + std::abs(x))) {
            double fxn = f(xn);
            if (is_valid(fxn) && std::abs(fxn) <= std::abs(fx)) return {xn, true, std::abs(fxn), it};
            return {x, true, std::abs(fx), it};
        }
        double fxn = f(xn);
        unsigned damp = 0;
        while ((!is_valid(fxn) || std::abs(fxn) > 2.0 * std::abs(fx)) && damp < 6) {
            dx *= 0.5;
            xn  = x - dx;
            fxn = f(xn);
            ++damp;
        }
        x  = xn;
        fx = is_valid(fxn) ? fxn : f(x);
        if (!is_valid(fx)) return {x, false, std::numeric_limits<double>::infinity(), it};
    }

    return {x, is_root(fx, tol * 10), std::abs(fx), max_iter};
}

auto EquationSolver::hybrid_solve(
    const Fn& f, const Fn& df,
    double a, double b,
    double tol, std::uint64_t max_iter
) -> SolverResult {
    auto bracket = brent(f, a, b, tol * 100.0, max_iter / 2);
    if (!bracket.converged) return bracket;
    auto polished = halley_newton(f, df, bracket.value, tol, max_iter / 2);
    if (polished.converged && polished.residual <= bracket.residual) {
        polished.iterations += bracket.iterations;
        return polished;
    }
    return bracket;
}

std::vector<double> EquationSolver::extrema_near_zero(
    const Fn& f, const Fn& df,
    const Interval& domain,
    std::uint64_t N, double tol
) {
    double lo = finite_lo(domain), hi = finite_hi(domain);
    std::vector<double> out;
    double step = (hi - lo) / static_cast<double>(N);
    double px = lo, pd = df(lo);
    for (std::uint64_t i = 1; i <= N; ++i) {
        double cx = (i == N) ? hi : lo + static_cast<double>(i) * step;
        double cd = df(cx);
        if (is_valid(pd) && is_valid(cd) && sign_change(pd, cd)) {
            double a = px, b = cx;
            double da = pd, db = cd;
            for (unsigned r = 0; r < 60; ++r) {
                double m  = (a + b) * 0.5;
                double dm = df(m);
                if (!is_valid(dm) || std::abs(b - a) < tol) break;
                if (sign_change(da, dm)) { b = m; db = dm; }
                else                     { a = m; da = dm; }
            }
            double ex  = (a + b) * 0.5;
            double fex = f(ex);
            if (is_valid(fex) && std::abs(fex) < tol * 1e6) out.push_back(ex);
        }
        px = cx; pd = cd;
    }
    return out;
}

void EquationSolver::try_add(
    std::vector<SolverResult>& results,
    const SolverResult& r,
    const Interval& domain,
    double tol
) {
    if (!r.converged) return;
    if (!domain.contains(r.value)) return;
    if (is_duplicate(results, r.value, tol)) return;
    results.push_back(r);
}

auto EquationSolver::find_all_impl(
    const Fn& f, const Fn& df,
    const Interval& domain,
    const SolverOptions& opts
) -> std::vector<SolverResult> {
    double tol = clamp_tol(opts.tolerance);
    if (domain.is_empty()) return {{std::numeric_limits<double>::quiet_NaN(), false, 0.0, 0}};
    double lo = finite_lo(domain);
    double hi = finite_hi(domain);
    std::vector<SolverResult> results;
    auto intervals = scan_intervals(f, domain, opts);
    for (auto& iv : intervals) {
        double a = std::max(iv.lower.value, lo);
        double b = std::min(iv.upper.value, hi);
        auto r = hybrid_solve(f, df, a, b, tol, opts.max_iterations);
        try_add(results, r, domain, tol);
    }
    auto ext = extrema_near_zero(f, df, domain, opts.coarse_scan_points, tol);
    for (double x : ext) {
        auto r = halley_newton(f, df, x, tol, opts.max_iterations);
        if (!r.converged && std::abs(f(x)) < tol * 1e4) {
            r.value     = x;
            r.converged = true;
            r.residual  = std::abs(f(x));
        }
        try_add(results, r, domain, tol);
    }
    std::sort(results.begin(), results.end(), [](auto& a, auto& b){ return a.value < b.value; });
    if (results.empty()) results.push_back({std::numeric_limits<double>::quiet_NaN(), false, 0.0, 0});
    return results;
}

auto EquationSolver::find_nearest_impl(
    const Fn& f, const Fn& df,
    double guess,
    const SolverOptions& opts
) -> SolverResult {
    double tol = clamp_tol(opts.tolerance);
    auto r = halley_newton(f, df, guess, tol, opts.max_iterations);
    if (r.converged) return r;
    double radius = 1.0;
    for (unsigned expand = 0; expand < 30; ++expand) {
        double a = guess - radius, b = guess + radius;
        double fa = f(a), fb = f(b);
        if (sign_change(fa, fb)) {
            auto br = hybrid_solve(f, df, a, b, tol, opts.max_iterations);
            if (br.converged) return br;
        }
        double fm = f(guess);
        if (is_valid(fa) && is_valid(fm) && sign_change(fa, fm)) {
            auto br = hybrid_solve(f, df, a, guess, tol, opts.max_iterations);
            if (br.converged) return br;
        }
        if (is_valid(fm) && is_valid(fb) && sign_change(fm, fb)) {
            auto br = hybrid_solve(f, df, guess, b, tol, opts.max_iterations);
            if (br.converged) return br;
        }
        radius *= 2.5;
    }
    return r;
}

auto EquationSolver::classify_critical_points_1d(
    cas::SymbolicContext& ctx,
    const cas::Expression& f_expr,
    const cas::Expression& fpp_expr,
    const std::string& var,
    const std::vector<SolverResult>& solutions
) -> std::vector<CriticalPoint1D> {
    auto eval = [&](const cas::Expression& e, double x) -> double { return e.evaluate({{var, x}}); };
    std::vector<CriticalPoint1D> out;
    out.reserve(solutions.size());

    for (auto& sol : solutions) {
        if (!sol.converged) continue;
        double px  = sol.value;
        double val = eval(f_expr, px);
        double fpp = eval(fpp_expr, px);
        CriticalPointType type = CriticalPointType::Inconclusive;

        if (is_valid(fpp)) {
            double tol_classify = std::max(1e-10, constants::type_epsilon() * std::abs(fpp));
            if (fpp > tol_classify)       type = CriticalPointType::Min;
            else if (fpp < -tol_classify) type = CriticalPointType::Max;
            else                          type = CriticalPointType::Inconclusive;
        }

        out.push_back({px, type, val, sol.residual, sol.converged});
    }

    return out;
}

auto EquationSolver::classify_inflection_points(
    cas::SymbolicContext& ctx,
    const cas::Expression& f_expr,
    const cas::Expression& fppp_expr,
    const std::string& var,
    const std::vector<SolverResult>& solutions
) -> std::vector<CriticalPoint1D> {
    auto eval = [&](const cas::Expression& e, double x) -> double { return e.evaluate({{var, x}}); };
    std::vector<CriticalPoint1D> out;
    out.reserve(solutions.size());

    for (auto& sol : solutions) {
        if (!sol.converged) continue;
        double px   = sol.value;
        double val  = eval(f_expr, px);
        double fppp = eval(fppp_expr, px);
        bool confirmed = is_valid(fppp) && std::abs(fppp) > std::max(1e-10, constants::type_epsilon() * std::abs(fppp));
        if (confirmed) { out.push_back({px, CriticalPointType::Inflection, val, sol.residual, sol.converged}); }
    }

    return out;
}

auto EquationSolver::filter_by_type(std::vector<CriticalPoint1D> pts, CriticalPointType type) -> std::vector<CriticalPoint1D> {
    pts.erase(
        std::remove_if(
            pts.begin(), pts.end(),
            [type](const CriticalPoint1D& cp) { return cp.type != type; }
        ),
        pts.end()
    );
    return pts;
}

} // namespace solvers
} // namespace math
} // namespace fizmo
