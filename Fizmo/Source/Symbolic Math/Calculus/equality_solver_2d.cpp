#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace solvers {

CriticalCurve2D::CriticalCurve2D() : constraint()
        , free_variable()
        , fixed_variable()
        , fixed_value(0.0)
        , free_range(Endpoint::closed(0.0), Endpoint::closed(0.0))
    {}

auto EquationSolver2D::solve(
    cas::SymbolicContext& ctx,
    const cas::Expression& f_expr, const cas::Expression& g_expr,
    const std::string& var_x, const std::string& var_y,
    const Region& domain,
    const SolverOptions2D& opts 
) -> std::vector<SolverResult2D> {
    auto pack = make_system_lambdas(ctx, f_expr, g_expr, var_x, var_y);
    return solve_impl(pack, domain, opts);
}

auto EquationSolver2D::find_critical_points(
    cas::SymbolicContext& ctx,
    const cas::Expression& f_expr,
    const std::string& var_x, const std::string& var_y,
    const Region& domain,
    const SolverOptions2D& opts 
) -> std::vector<CriticalPoint2D> {
    cas::Expression fx  = ctx.derivative(f_expr, var_x);
    cas::Expression fy  = ctx.derivative(f_expr, var_y);
    cas::Expression fxy = ctx.derivative(fx, var_y);
    auto solutions = solve(ctx, fx, fy, var_x, var_y, domain, opts);
    auto classified = classify_critical_points(ctx, f_expr, fx, fy, fxy, var_x, var_y, solutions);
    return collapse_degenerate_clusters(classified);        
}

auto EquationSolver2D::find_local_minima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts 
) -> std::vector<CriticalPoint2D> { return filter_by_type(find_critical_points(ctx, f, vx, vy, domain, opts), CriticalPointType::Min); }

auto EquationSolver2D::find_local_maxima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts 
) -> std::vector<CriticalPoint2D> { return filter_by_type(find_critical_points(ctx, f, vx, vy, domain, opts), CriticalPointType::Max); }

auto EquationSolver2D::find_saddle_points(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts 
) -> std::vector<CriticalPoint2D> { return filter_by_type(find_critical_points(ctx, f, vx, vy, domain, opts), CriticalPointType::Saddle); }

auto EquationSolver2D::analyze_critical_structure(
    cas::SymbolicContext& ctx,
    const cas::Expression& f_expr,
    const std::string& var_x, const std::string& var_y,
    const Region& domain,
    const SolverOptions2D& opts 
) -> CriticalAnalysis2D {
    cas::Expression fx  = ctx.derivative(f_expr, var_x);
    cas::Expression fy  = ctx.derivative(f_expr, var_y);
    cas::Expression fxy = ctx.derivative(fx, var_y);
    auto solutions = solve(ctx, fx, fy, var_x, var_y, domain, opts);
    auto classified = classify_critical_points(ctx, f_expr, fx, fy, fxy, var_x, var_y, solutions);
    return detect_critical_curves(fx, fy, var_x, var_y, domain, classified);
}

auto EquationSolver2D::analyze_critical_structure(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts 
) -> CriticalAnalysis2D { return analyze_critical_structure(ctx, f_expr, var_x, var_y, Region::rectangle(x0, x1, y0, y1), opts); }

auto EquationSolver2D::analyze_critical_structure(
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts 
) -> CriticalAnalysis2D { return analyze_critical_structure(cas::global_context(), f_expr, var_x, var_y, x0, x1, y0, y1, opts); }

double EquationSolver2D::combined_residual(double fv, double gv) noexcept {
    if (!is_valid(fv) || !is_valid(gv)) return std::numeric_limits<double>::infinity();
    return std::sqrt(fv * fv + gv * gv);
}

bool EquationSolver2D::is_duplicate(
    const std::vector<SolverResult2D>& results,
    double px, double py, double tol
) {
    double merge_dist = tol * 10.0;
    for (auto& r : results) {
        double dx = r.point.x - px, dy = r.point.y - py;
        if (std::sqrt(dx * dx + dy * dy) < merge_dist) return true;
    }
    return false;
}

auto EquationSolver2D::make_system_lambdas(
    cas::SymbolicContext& ctx,
    const cas::Expression& f_expr, const cas::Expression& g_expr,
    const std::string& vx, const std::string& vy
) -> SystemPack {
    cas::Expression dfx = ctx.derivative(f_expr, vx);
    cas::Expression dfy = ctx.derivative(f_expr, vy);
    cas::Expression dgx = ctx.derivative(g_expr, vx);
    cas::Expression dgy = ctx.derivative(g_expr, vy);

    auto make_fn = [&](const cas::Expression& e) -> Fn2 {
        return [e, vx, vy](double x, double y) -> double {
            return e.evaluate({{vx, x}, {vy, y}});
        };
    };

    return {
        make_fn(f_expr), make_fn(g_expr),
        make_fn(dfx),    make_fn(dfy),
        make_fn(dgx),    make_fn(dgy)
    };
}

auto EquationSolver2D::newton2d(
    const SystemPack& sys,
    double x0, double y0,
    double tol, std::uint64_t max_iter
) -> SolverResult2D {
    double x = x0, y = y0;
    double fv = sys.f(x, y), gv = sys.g(x, y);
    if (!is_valid(fv) || !is_valid(gv)) return {{x, y}, false, std::numeric_limits<double>::infinity(), 0};

    for (std::uint64_t it = 0; it < max_iter; ++it) {
        double res = combined_residual(fv, gv);
        if (res <= tol) return {{x, y}, true, res, it};
        double j00 = sys.df_dx(x, y), j01 = sys.df_dy(x, y);
        double j10 = sys.dg_dx(x, y), j11 = sys.dg_dy(x, y);

        if (!is_valid(j00) || !is_valid(j01) || !is_valid(j10) || !is_valid(j11)) {
            double h = std::max(1e-8, std::max(std::abs(x), std::abs(y)) * 1e-7);
            j00 = (sys.f(x + h, y) - sys.f(x - h, y)) / (2.0 * h);
            j01 = (sys.f(x, y + h) - sys.f(x, y - h)) / (2.0 * h);
            j10 = (sys.g(x + h, y) - sys.g(x - h, y)) / (2.0 * h);
            j11 = (sys.g(x, y + h) - sys.g(x, y - h)) / (2.0 * h);
            if (!is_valid(j00) || !is_valid(j01) || !is_valid(j10) || !is_valid(j11)) return {{x, y}, false, res, it};
        }

        double det = j00 * j11 - j01 * j10;
        if (std::abs(det) < constants::type_epsilon()) {
            double h = std::max(std::abs(x), std::abs(y)) * 1e-6 + 1e-10;
            x += h; y += h * 0.5;
            fv = sys.f(x, y); gv = sys.g(x, y);
            if (!is_valid(fv) || !is_valid(gv)) return {{x, y}, false, std::numeric_limits<double>::infinity(), it};
            continue;
        }

        double inv_det = 1.0 / det;
        double dx = inv_det * ( j11 * fv - j01 * gv);
        double dy = inv_det * (-j10 * fv + j00 * gv);
        double xn = x - dx, yn = y - dy;
        double fn = sys.f(xn, yn), gn = sys.g(xn, yn);
        unsigned damp = 0;

        while ((!is_valid(fn) || !is_valid(gn) || combined_residual(fn, gn) > 2.0 * res) && damp < 8) {
            dx *= 0.5; dy *= 0.5;
            xn = x - dx; yn = y - dy;
            fn = sys.f(xn, yn); gn = sys.g(xn, yn);
            ++damp;
        }

        if (!is_valid(fn) || !is_valid(gn)) return {{x, y}, false, res, it};

        if (std::abs(dx) < tol * (1.0 + std::abs(x)) && std::abs(dy) < tol * (1.0 + std::abs(y))) {
            double final_res = combined_residual(fn, gn);
            if (final_res <= res) return {{xn, yn}, true, final_res, it};
            return {{x, y}, true, res, it};
        }

        x = xn; y = yn; fv = fn; gv = gn;
    }

    double res = combined_residual(fv, gv);
    return {{x, y}, res <= tol * 10.0, res, max_iter};
}

double EquationSolver2D::score_cell(
    const SystemPack& sys,
    double cx0, double cy0, double cx1, double cy1
) {
    double mx = (cx0 + cx1) * 0.5, my = (cy0 + cy1) * 0.5;

    struct { double x, y; } pts[5] = {
        {cx0, cy0}, {cx1, cy0}, {cx0, cy1}, {cx1, cy1}, {mx, my}
    };

    double fvals[5], gvals[5];
    for (int i = 0; i < 5; ++i) {
        fvals[i] = sys.f(pts[i].x, pts[i].y);
        gvals[i] = sys.g(pts[i].x, pts[i].y);
    }

    double best_res = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 5; ++i) {
        double r = combined_residual(fvals[i], gvals[i]);
        if (r < best_res) best_res = r;
    }

    bool f_sign_change = false, g_sign_change = false;
    for (int i = 0; i < 4 && !(f_sign_change && g_sign_change); ++i) {
        for (int j = i + 1; j < 4; ++j) {
            if (is_valid(fvals[i]) && is_valid(fvals[j]) && ((fvals[i] > 0) != (fvals[j] > 0))) f_sign_change = true;
            if (is_valid(gvals[i]) && is_valid(gvals[j]) && ((gvals[i] > 0) != (gvals[j] > 0))) g_sign_change = true;
        }
    }

    if (f_sign_change && g_sign_change) return 1.0 / (best_res + constants::type_epsilon());
    if (f_sign_change || g_sign_change) return 0.5 / (best_res + constants::type_epsilon());
    if (best_res < 1.0) return 0.25 / (best_res + constants::type_epsilon());
    return 0.0;
}

auto EquationSolver2D::solve_impl(
    const SystemPack& sys,
    const Region& domain,
    const SolverOptions2D& opts
) -> std::vector<SolverResult2D> {
    double tol = clamp_tol(opts.tolerance);
    if (domain.is_empty()) return {};
    double xlo = finite_bound(domain.x_range.lower.value, -1e300);
    double xhi = finite_bound(domain.x_range.upper.value,  1e300);
    double ylo = finite_bound(domain.y_range.lower.value, -1e300);
    double yhi = finite_bound(domain.y_range.upper.value,  1e300);
    std::uint64_t nx = opts.grid_nx, ny = opts.grid_ny;
    double dx = (xhi - xlo) / static_cast<double>(nx);
    double dy = (yhi - ylo) / static_cast<double>(ny);
    std::priority_queue<Cell> pq;

    for (std::uint64_t ix = 0; ix < nx; ++ix) {
        for (std::uint64_t iy = 0; iy < ny; ++iy) {
            double cx0 = xlo + ix * dx;
            double cy0 = ylo + iy * dy;
            double cx1 = (ix + 1 == nx) ? xhi : cx0 + dx;
            double cy1 = (iy + 1 == ny) ? yhi : cy0 + dy;
            double s = score_cell(sys, cx0, cy0, cx1, cy1);
            if (s > opts.adaptive_score_cutoff) pq.push({cx0, cy0, cx1, cy1, s});
        }
    }

    std::vector<Cell> candidate_cells;
    candidate_cells.reserve(opts.max_adaptive_cells);
    std::uint64_t cells_processed = 0;

    while (!pq.empty() && cells_processed < opts.max_adaptive_cells) {
        Cell cell = pq.top(); pq.pop();
        double cw = cell.x1 - cell.x0, ch = cell.y1 - cell.y0;

        if (cw < dx * 0.25 && ch < dy * 0.25) {
            candidate_cells.push_back(cell);
            ++cells_processed;
            continue;
        }

        double mx = (cell.x0 + cell.x1) * 0.5;
        double my = (cell.y0 + cell.y1) * 0.5;
        struct { double x0, y0, x1, y1; } children[4] = {
            {cell.x0, cell.y0, mx,      my},
            {mx,      cell.y0, cell.x1, my},
            {cell.x0, my,      mx,      cell.y1},
            {mx,      my,      cell.x1, cell.y1}
        };

        for (auto& ch4 : children) {
            double s = score_cell(sys, ch4.x0, ch4.y0, ch4.x1, ch4.y1);
            if (s > opts.adaptive_score_cutoff * 0.5) pq.push({ch4.x0, ch4.y0, ch4.x1, ch4.y1, s});
        }
        ++cells_processed;
    }

    while (!pq.empty() && candidate_cells.size() < opts.max_adaptive_cells) {
        candidate_cells.push_back(pq.top());
        pq.pop();
    }

    std::vector<SolverResult2D> results;
    results.reserve(candidate_cells.size());

    for (auto& cell : candidate_cells) {
        double mx = (cell.x0 + cell.x1) * 0.5;
        double my = (cell.y0 + cell.y1) * 0.5;
        auto r = newton2d(sys, mx, my, tol, opts.max_iterations);

        if (r.converged && domain.contains(r.point.x, r.point.y) && !is_duplicate(results, r.point.x, r.point.y, tol)) {
            results.push_back(r);
        }
    }

    std::uint64_t sparse_nx = std::min(nx, std::uint64_t(32));
    std::uint64_t sparse_ny = std::min(ny, std::uint64_t(32));
    double sdx = (xhi - xlo) / static_cast<double>(sparse_nx);
    double sdy = (yhi - ylo) / static_cast<double>(sparse_ny);

    for (std::uint64_t ix = 0; ix <= sparse_nx; ++ix) {
        for (std::uint64_t iy = 0; iy <= sparse_ny; ++iy) {
            double sx = xlo + ix * sdx;
            double sy = ylo + iy * sdy;
            double fv = sys.f(sx, sy), gv = sys.g(sx, sy);
            if (combined_residual(fv, gv) < tol * 1e6) {
                auto r = newton2d(sys, sx, sy, tol, opts.max_iterations);
                if (r.converged && domain.contains(r.point.x, r.point.y) && !is_duplicate(results, r.point.x, r.point.y, tol)) {
                    results.push_back(r);
                }
            }
        }
    }

    std::sort(results.begin(), results.end(), [](auto& a, auto& b) {
        if (std::abs(a.point.x - b.point.x) > 1e-12) return a.point.x < b.point.x;
        return a.point.y < b.point.y;
    });

    return results;
}

auto EquationSolver2D::filter_by_type(
    std::vector<CriticalPoint2D> pts,
    CriticalPointType type
) -> std::vector<CriticalPoint2D> {
    pts.erase(
        std::remove_if(
            pts.begin(), pts.end(),
            [type](const CriticalPoint2D& cp) { return cp.type != type; }
        ),
        pts.end()
    );
    return pts;
}

auto EquationSolver2D::classify_critical_points(
    cas::SymbolicContext& ctx,
    const cas::Expression& f,
    const cas::Expression& fx, const cas::Expression& fy, const cas::Expression& fxy,
    const std::string& varx, const std::string& vary,
    const std::vector<SolverResult2D>& solutions
) -> std::vector<CriticalPoint2D> {
    cas::Expression fxx = ctx.derivative(fx, varx);
    cas::Expression fyy = ctx.derivative(fy, vary);
    Fn2 f_   = [&](double x, double y) { return   f.evaluate({{varx, x}, {vary, y}}); };
    Fn2 fxx_ = [&](double x, double y) { return fxx.evaluate({{varx, x}, {vary, y}}); };
    Fn2 fyy_ = [&](double x, double y) { return fyy.evaluate({{varx, x}, {vary, y}}); };
    Fn2 fxy_ = [&](double x, double y) { return fxy.evaluate({{varx, x}, {vary, y}}); };
    std::vector<CriticalPoint2D> result;
    result.reserve(solutions.size());

    for (const auto& sol : solutions) {
        double px   = sol.point.x;
        double py   = sol.point.y;
        double fval = f_(px, py);
        CriticalPointType type = try_hessian(fxx_, fyy_, fxy_, px, py);
        if (type == CriticalPointType::Inconclusive) type = try_axis_slices(f_, fval, px, py);
        if (type == CriticalPointType::Inconclusive) type = try_generic_lines(f_, fval, px, py);
        if (type == CriticalPointType::Inconclusive) type = try_taylor(f_, fxx_, fyy_, fxy_, fval, px, py);
        if (type == CriticalPointType::Inconclusive) type = try_shrinking_circle(f_, fval, px, py);
        result.push_back({ sol.point, type, fval, sol.residual, sol.converged });
    }

    result.shrink_to_fit();
    return result;
}

auto EquationSolver2D::classify_1d_slice(
    const Fn2& f, double fval,
    double px, double py,
    double dx, double dy,
    double h
) -> SliceVerdict {
    constexpr double offsets[] = { 
        -2.0, -1.75, -1.5, -1.25, -1.0, -0.75, -0.5, -0.25, 
        0.25, 0.5, 1.0, 1.25, 1.5, 1.75, 2.0 
    };

    double tol = constants::middle_epsilon() * std::max(1.0, std::abs(fval));
    bool all_above = true, all_below = true;

    for (double s : offsets) {
        double t = s * h;
        double v = f(px + t * dx, py + t * dy);
        if (!is_valid(v)) return SliceVerdict::Inconclusive;
        double diff = v - fval;
        if (diff <=  tol) all_above = false;
        if (diff >= -tol) all_below = false;
    }

    if (all_above) return SliceVerdict::Min;
    if (all_below) return SliceVerdict::Max;
    return SliceVerdict::Neither;
}

auto EquationSolver2D::try_hessian(
    const Fn2& fxx, const Fn2& fyy, const Fn2& fxy,
    double px, double py
) -> CriticalPointType {
    double h11 = fxx(px, py);
    double h22 = fyy(px, py);
    double h12 = fxy(px, py);
    if (!is_valid(h11) || !is_valid(h22) || !is_valid(h12)) return CriticalPointType::Inconclusive;
    double det   = h11 * h22 - h12 * h12;
    double scale = std::max({ std::abs(h11), std::abs(h22), std::abs(h12), 1.0 });
    double tol   = scale * constants::middle_epsilon();
    if (std::abs(det) < tol)  return CriticalPointType::Inconclusive;
    if (det < 0.0)            return CriticalPointType::Saddle;
    if (h11 >  tol)           return CriticalPointType::Min;
    if (h11 < -tol)           return CriticalPointType::Max;
    return CriticalPointType::Inconclusive;
}

auto EquationSolver2D::try_axis_slices(
    const Fn2& f, double fval,
    double px, double py
) -> CriticalPointType {
    double h  = probe_step(px, py);
    auto sx = classify_1d_slice(f, fval, px, py, 1.0, 0.0, h);
    auto sy = classify_1d_slice(f, fval, px, py, 0.0, 1.0, h);
    if (sx == SliceVerdict::Inconclusive || sy == SliceVerdict::Inconclusive) return CriticalPointType::Inconclusive;
    if (sx == SliceVerdict::Min && sy == SliceVerdict::Min) return CriticalPointType::Min;
    if (sx == SliceVerdict::Max && sy == SliceVerdict::Max) return CriticalPointType::Max;
    return CriticalPointType::Saddle;
}

auto EquationSolver2D::try_generic_lines(
    const Fn2& f, double fval,
    double px, double py
) -> CriticalPointType {
    double h = probe_step(px, py);
    constexpr double slopes[] = { 1.0, -1.0, 2.0, -2.0, 0.5, -0.5 };

    int min_votes = 0, max_votes = 0, neither_votes = 0;

    for (double m : slopes) {
        double norm = std::sqrt(1.0 + m * m);
        double dx = 1.0 / norm, dy = m / norm;
        auto v = classify_1d_slice(f, fval, px, py, dx, dy, h);
        switch (v) {
            case SliceVerdict::Min:          ++min_votes;     break;
            case SliceVerdict::Max:          ++max_votes;     break;
            case SliceVerdict::Neither:      ++neither_votes; break;
            case SliceVerdict::Inconclusive:                  break;
        }
    }

    int total = min_votes + max_votes + neither_votes;
    if (total < 3) return CriticalPointType::Inconclusive;
    if ((min_votes > 0 && max_votes > 0) || neither_votes > 0) return CriticalPointType::Saddle;
    if (min_votes >= 3) return CriticalPointType::Min;
    if (max_votes >= 3) return CriticalPointType::Max;
    return CriticalPointType::Inconclusive;
}

auto EquationSolver2D::try_taylor(
    const Fn2& f,
    const Fn2& fxx, const Fn2& fyy, const Fn2& fxy,
    double fval, double px, double py
) -> CriticalPointType {
    double h11 = fxx(px, py), h22 = fyy(px, py), h12 = fxy(px, py);
    if (!is_valid(h11) || !is_valid(h22) || !is_valid(h12)) return CriticalPointType::Inconclusive;
    double tr   = h11 + h22;
    double disc = (h11 - h22) * (h11 - h22) + 4.0 * h12 * h12;
    if (disc < 0.0) return CriticalPointType::Inconclusive;
    double sq   = std::sqrt(disc);
    double lam1 = (tr + sq) * 0.5;
    double lam2 = (tr - sq) * 0.5;
    double small_lam, big_lam;
        
    if (std::abs(lam1) < std::abs(lam2)) { 
        small_lam = lam1; big_lam = lam2; 
    } else { 
        small_lam = lam2; big_lam = lam1;
    }

    double scale = std::max({ std::abs(h11), std::abs(h22), std::abs(h12), 1.0 });
    if (std::abs(small_lam) > scale * 1e-6) return CriticalPointType::Inconclusive;
    double ndx, ndy;

    if (std::abs(h12) > constants::type_epsilon()) {
        ndx = h12;
        ndy = small_lam - h11;
    } else {
        if (std::abs(h11) < std::abs(h22)) { 
            ndx = 1.0; ndy = 0.0; 
        } else { 
            ndx = 0.0; ndy = 1.0; 
        }
    }

    double norm = std::sqrt(ndx * ndx + ndy * ndy);
    if (norm < constants::type_epsilon()) return CriticalPointType::Inconclusive;
    ndx /= norm;
    ndy /= norm;
    constexpr double step_scales[] = { 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8, 1e-9, 1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15 };
    double base = std::max(1.0, std::max(std::abs(px), std::abs(py)));
    double tol  = constants::middle_epsilon() * std::max(1.0, std::abs(fval));

    for (double ss : step_scales) {
        double h = base * ss;
        double fp = f(px + h * ndx, py + h * ndy) - fval;
        double fm = f(px - h * ndx, py - h * ndy) - fval;
        if (!is_valid(fp) || !is_valid(fm)) continue;
        if ((fp > tol && fm < -tol) || (fp < -tol && fm > tol)) return CriticalPointType::Saddle;

        if (fp > tol && fm > tol) {
            if (big_lam >  scale * constants::middle_epsilon()) return CriticalPointType::Min;
            if (big_lam < -scale * constants::middle_epsilon()) return CriticalPointType::Saddle;
            return CriticalPointType::Inconclusive;
        }

        if (fp < -tol && fm < -tol) {
            if (big_lam < -scale * constants::middle_epsilon()) return CriticalPointType::Max;
            if (big_lam >  scale * constants::middle_epsilon()) return CriticalPointType::Saddle;
            return CriticalPointType::Inconclusive;
        }
        // Both ~ 0 at this scale, try a larger step
    }

    return CriticalPointType::Inconclusive;
}

auto EquationSolver2D::try_shrinking_circle(
    const Fn2& f, double fval,
    double px, double py
) -> CriticalPointType {
    constexpr int    NANGLES  = 24;
    constexpr int    NLEVELS  = 6;
    constexpr double TWO_PI   = 2.0 * constants::pi();
    double base_r = std::max(1e-4, std::max(std::abs(px), std::abs(py)) * 1e-3);

    int min_votes = 0, max_votes = 0, saddle_votes = 0;

    for (int level = 0; level < NLEVELS; ++level) {
        double r = base_r * std::pow(0.1, level);
        double tol = constants::middle_epsilon() * std::max(1.0, std::abs(fval));
        bool has_above = false, has_below = false;
        bool valid = true;

        for (int i = 0; i < NANGLES; ++i) {
            double theta = TWO_PI * i / NANGLES;
            double v = f(px + r * std::cos(theta), py + r * std::sin(theta));
            if (!is_valid(v)) { valid = false; break; }
            double diff = v - fval;
            if (diff >  tol) has_above = true;
            if (diff < -tol) has_below = true;
        }

        if (!valid) continue;

        if (has_above && has_below) ++saddle_votes;
        else if (has_above)         ++min_votes;
        else if (has_below)         ++max_votes;
    }

    if (saddle_votes >= 1) return CriticalPointType::Saddle;
    if (min_votes >= 3 && max_votes == 0) return CriticalPointType::Min;
    if (max_votes >= 3 && min_votes == 0) return CriticalPointType::Max;
    return CriticalPointType::Inconclusive;
}

auto EquationSolver2D::cluster_by_coordinate(
    const std::vector<CriticalPoint2D>& pts,
    bool by_x
) -> std::vector<CoordGroup> {
    struct Entry { double val; std::size_t idx; };
    std::vector<Entry> entries;
    entries.reserve(pts.size());
    for (std::size_t i = 0; i < pts.size(); ++i) entries.push_back({ by_x ? pts[i].point.x : pts[i].point.y, i });
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.val < b.val; });

    constexpr double REL_TOL = 1e-6;
    std::vector<CoordGroup> groups;
    std::size_t i = 0;
    while (i < entries.size()) {
        CoordGroup g;
        g.indices.push_back(entries[i].idx);
        std::size_t j = i + 1;
        while (j < entries.size()) {
            double gap   = entries[j].val - entries[j - 1].val;
            double scale = std::max(1.0, std::abs(entries[j].val));
            if (gap > scale * REL_TOL) break;
            g.indices.push_back(entries[j].idx);
            ++j;
        }
        std::size_t mid = i + (j - i) / 2;
        g.representative = entries[mid].val;
        groups.push_back(std::move(g));
        i = j;
    }
    return groups;
}

double EquationSolver2D::snap_to_nice(double v, double tol) noexcept {
    if (std::abs(v) < tol) return 0.0;
    double rounded = std::round(v);
    if (std::abs(v - rounded) < tol) return rounded;
    double half = std::round(v * 2.0) / 2.0;
    if (std::abs(v - half) < tol) return half;
    return v;
}

bool EquationSolver2D::verify_critical_line(
    const cas::Expression& fx, const cas::Expression& fy,
    const std::string& var_x, const std::string& var_y,
    double fixed_val, bool x_is_fixed,
    const Region& domain
) {
    const Interval& free_iv = x_is_fixed ? domain.y_range : domain.x_range;
    double lo = free_iv.lower.is_negative_infinity() ? -100.0 : free_iv.lower.value;
    double hi = free_iv.upper.is_positive_infinity() ?  100.0 : free_iv.upper.value;
    constexpr size_t NUM_PROBES = 25;
    constexpr double VERIFY_TOL = 1e-6;
    std::size_t pass = 0;

    for (std::size_t i = 0; i <= NUM_PROBES; ++i) {
        double t    = lo + (hi - lo) * static_cast<double>(i) / NUM_PROBES;
        double xv   = x_is_fixed ? fixed_val : t;
        double yv   = x_is_fixed ? t : fixed_val;
        double fx_v = fx.evaluate({{ var_x, xv }, { var_y, yv }});
        double fy_v = fy.evaluate({{ var_x, xv }, { var_y, yv }});
        if (!std::isfinite(fx_v) || !std::isfinite(fy_v)) continue;
        if (std::abs(fx_v) < VERIFY_TOL && std::abs(fy_v) < VERIFY_TOL) ++pass;
    }
    return pass >= (NUM_PROBES * 3) / 4;
}

auto EquationSolver2D::build_axis_line_curve(
    const std::string& fixed_var,
    const std::string& free_var,
    double fixed_val,
    const Interval& free_iv
) -> CriticalCurve2D {
    CriticalCurve2D curve;
    curve.fixed_variable = fixed_var;
    curve.free_variable  = free_var;
    curve.fixed_value    = fixed_val;
    curve.free_range     = free_iv;
    curve.constraint = cas::VARIABLE(fixed_var) - fixed_val;
    double lo = free_iv.lower.is_negative_infinity() ? -100.0 : free_iv.lower.value;
    double hi = free_iv.upper.is_positive_infinity() ?  100.0 : free_iv.upper.value;
    constexpr std::size_t NUM_SAMPLES = 5;
    for (std::size_t i = 0; i <= NUM_SAMPLES; ++i) {
        double t = lo + (hi - lo) * static_cast<double>(i) / NUM_SAMPLES;
        if (fixed_var < free_var)          // lexicographic: fixed_var is x
            curve.samples.push_back({ fixed_val, t });
        else
            curve.samples.push_back({ t, fixed_val });
    }
    return curve;
}

auto EquationSolver2D::detect_critical_curves(
    const cas::Expression& fx, const cas::Expression& fy,
    const std::string& var_x, const std::string& var_y,
    const Region& domain,
    const std::vector<CriticalPoint2D>& points
) -> CriticalAnalysis2D {
    CriticalAnalysis2D analysis;
    constexpr size_t CURVE_THRESHOLD = 15;

    if (points.size() < CURVE_THRESHOLD) {
        analysis.isolated = points;
        return analysis;
    }

    std::vector<bool> consumed(points.size(), false);

    detect_axis_curves(
        points, consumed, fx, fy, var_x, var_y, domain,
        /*by_x=*/true, analysis
    );
    detect_axis_curves(
        points, consumed, fx, fy, var_x, var_y, domain,
        /*by_x=*/false, analysis
    );
    for (size_t i = 0; i < points.size(); ++i) if (!consumed[i]) analysis.isolated.push_back(points[i]);
    return analysis;
}

void EquationSolver2D::detect_axis_curves(
    const std::vector<CriticalPoint2D>& points,
    std::vector<bool>& consumed,
    const cas::Expression& fx, const cas::Expression& fy,
    const std::string& var_x, const std::string& var_y,
    const Region& domain,
    bool by_x,
    CriticalAnalysis2D& analysis
) {
    constexpr std::size_t CURVE_THRESHOLD = 15;
    auto groups = cluster_by_coordinate(points, by_x);

    for (auto& group : groups) {
        std::size_t alive = 0;
        for (std::size_t idx : group.indices) if (!consumed[idx]) ++alive;
        if (alive < CURVE_THRESHOLD) continue;
        double snapped = snap_to_nice(group.representative);
        bool   x_fixed = by_x;
        if (!verify_critical_line(fx, fy, var_x, var_y, snapped, x_fixed, domain)) continue;
        const std::string& fixed_var = x_fixed ? var_x : var_y;
        const std::string& free_var  = x_fixed ? var_y : var_x;
        const Interval&    free_iv   = x_fixed ? domain.y_range : domain.x_range;

        analysis.curves.push_back(
            build_axis_line_curve(fixed_var, free_var, snapped, free_iv)
        );

        for (size_t idx : group.indices) consumed[idx] = true;
    }
}

auto EquationSolver2D::collapse_degenerate_clusters(
    const std::vector<CriticalPoint2D>& points
) -> std::vector<CriticalPoint2D> {
    constexpr std::size_t CURVE_THRESHOLD = 15;
    if (points.size() < CURVE_THRESHOLD) return points;
    std::vector<bool> consumed(points.size(), false);
    std::vector<CriticalPoint2D> result;
    collapse_axis(points, consumed, /*by_x=*/true, result);
    collapse_axis(points, consumed, /*by_x=*/false, result);
    for (std::size_t i = 0; i < points.size(); ++i) if (!consumed[i]) result.push_back(points[i]);
    return result;
}

void EquationSolver2D::collapse_axis(
    const std::vector<CriticalPoint2D>& points,
    std::vector<bool>& consumed,
    bool by_x,
    std::vector<CriticalPoint2D>& result
) {
    constexpr std::size_t CURVE_THRESHOLD = 15;
    auto groups = cluster_by_coordinate(points, by_x);

    for (auto& group : groups) {
        std::size_t alive = 0;
        for (std::size_t idx : group.indices) if (!consumed[idx]) ++alive;
        if (alive < CURVE_THRESHOLD) continue;

        std::vector<std::size_t> live;
        live.reserve(alive);
        for (std::size_t idx : group.indices) if (!consumed[idx]) live.push_back(idx);

        std::sort(live.begin(), live.end(), [&](std::size_t a, std::size_t b) {
            return by_x ? points[a].point.y < points[b].point.y : points[a].point.x < points[b].point.x;
        });

        size_t mid_idx = live[live.size() / 2];
        CriticalPoint2D rep = points[mid_idx];
        rep.type = CriticalPointType::Degenerate;

        if (by_x) rep.point.x = snap_to_nice(group.representative);
        else      rep.point.y = snap_to_nice(group.representative);

        result.push_back(rep);
        for (size_t idx : group.indices) consumed[idx] = true;
    }
}

} // namespace solvers
} // namespace math
} // namespace fizmo
