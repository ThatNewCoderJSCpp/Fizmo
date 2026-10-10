#ifndef FIZMO_EQUATION_SOLVER_2D_HPP
#define FIZMO_EQUATION_SOLVER_2D_HPP

#include "../CAS/expression_wrapper.hpp"
#include "../CAS/symbolic_context.hpp"
#include "../../Basic/constants.hpp"
#include "../Common/intervals.hpp"
#include "../Common/points.hpp"
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <functional>
#include <utility>
#include <array>
#include "../Common/intervals.hpp"
#include "../Common/main_convenience.hpp"
#include "../Common/points.hpp"
#include "equality_solver.hpp"

namespace fizmo {
namespace math {
namespace solvers {

struct SolverResult2D {
    Point2D<double> point;
    bool            converged;
    double          residual;      
    std::uint64_t   iterations;
};

struct SolverOptions2D {
    double        tolerance              = constants::middle_epsilon();
    std::uint64_t max_iterations         = 1ULL << 10;
    std::uint64_t grid_nx                = 1ULL << 7;
    std::uint64_t grid_ny                = 1ULL << 7;
    std::uint64_t max_adaptive_cells     = 1ULL << 13;
    std::uint64_t adaptive_refine_depth  = 1ULL << 3;
    double        adaptive_score_cutoff  = 0.1;  
};

struct CriticalPoint2D {
    Point2D<double>    point;
    CriticalPointType  type;
    double             function_value;
    double             residual;
    bool               converged;
};

struct CriticalCurve2D {
    cas::Expression  constraint;      
    std::string free_variable;   
    std::string fixed_variable; 
    double      fixed_value;     
    Interval    free_range;      
    std::vector<Point2D<>> samples;

    CriticalCurve2D()
;

    bool is_vertical_line()   const { return free_variable.size() && fixed_variable != free_variable; }
    bool is_horizontal_line() const { return free_variable.size() && fixed_variable != free_variable; }

    friend std::ostream& operator<<(std::ostream& os, const CriticalCurve2D& c) {
        os << "CriticalCurve(" << c.fixed_variable << " = " << c.fixed_value
           << ", " << c.free_variable << " in " << c.free_range.to_string() << ")";
        return os;
    }
};

struct CriticalAnalysis2D {
    std::vector<CriticalPoint2D> isolated;
    std::vector<CriticalCurve2D> curves;

    bool   has_curves()     const { return !curves.empty(); }
    bool   has_isolated()   const { return !isolated.empty(); }
    bool   is_degenerate()  const { return !curves.empty(); }
    std::size_t total_features() const { return isolated.size() + curves.size(); }

    friend std::ostream& operator<<(std::ostream& os, const CriticalAnalysis2D& a) {
        if (a.has_curves()) {
            os << "Critical curves (" << a.curves.size() << "):\n";
            for (auto& c : a.curves) os << "  " << c << "\n";
        }
        if (a.has_isolated()) {
            os << "Isolated critical points (" << a.isolated.size() << "):\n";
            for (auto& p : a.isolated) os << "  " << p.point << " | " << p.type << "\n";
        }
        if (!a.has_curves() && !a.has_isolated()) os << "No critical features found.\n";
        return os;
    }
};

class EquationSolver2D {
public:
    using Fn2  = std::function<double(double, double)>;

    static std::vector<SolverResult2D> solve(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr, const cas::Expression& g_expr,
        const std::string& var_x, const std::string& var_y,
        const Region& domain,
        const SolverOptions2D& opts = {}
    );

    static std::vector<SolverResult2D> solve(
        const cas::Expression& f_expr, const cas::Expression& g_expr,
        const std::string& var_x, const std::string& var_y,
        const Region& domain,
        const SolverOptions2D& opts = {}
    ) { return solve(cas::global_context(), f_expr, g_expr, var_x, var_y, domain, opts); }

    static std::vector<SolverResult2D> solve(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr, const cas::Expression& g_expr,
        const std::string& var_x, const std::string& var_y,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return solve(ctx, f_expr, g_expr, var_x, var_y, Region::rectangle(x0, x1, y0, y1), opts); }

    static std::vector<SolverResult2D> solve(
        const cas::Expression& f_expr, const cas::Expression& g_expr,
        const std::string& var_x, const std::string& var_y,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return solve(cas::global_context(), f_expr, g_expr, var_x, var_y, x0, x1, y0, y1, opts); }

    static std::vector<SolverResult2D> solve(
        const Fn2& f, const Fn2& g,
        const Fn2& df_dx, const Fn2& df_dy,
        const Fn2& dg_dx, const Fn2& dg_dy,
        const Region& domain,
        const SolverOptions2D& opts = {}
    ) {
        SystemPack pack{ f, g, df_dx, df_dy, dg_dx, dg_dy };
        return solve_impl(pack, domain, opts);
    }

    static std::vector<CriticalPoint2D> find_critical_points(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        const Region& domain,
        const SolverOptions2D& opts = {}
    );

    static std::vector<CriticalPoint2D> find_critical_points(
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        const Region& domain,
        const SolverOptions2D& opts = {}
    ) { return find_critical_points(cas::global_context(), f_expr, var_x, var_y, domain, opts); }

    static std::vector<CriticalPoint2D> find_critical_points(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return find_critical_points(ctx, f_expr, var_x, var_y, Region::rectangle(x0, x1, y0, y1), opts); }

    static std::vector<CriticalPoint2D> find_critical_points(
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return find_critical_points(cas::global_context(), f_expr, var_x, var_y, x0, x1, y0, y1, opts); }

    static std::vector<CriticalPoint2D> find_local_minima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts = {}
    );

    static std::vector<CriticalPoint2D> find_local_minima(
        const cas::Expression& f, const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts = {}
    ) { return find_local_minima(cas::global_context(), f, vx, vy, domain, opts); }

    static std::vector<CriticalPoint2D> find_local_minima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return find_local_minima(ctx, f, vx, vy, Region::rectangle(x0, x1, y0, y1), opts); }

    static std::vector<CriticalPoint2D> find_local_minima(
        const cas::Expression& f, const std::string& vx, const std::string& vy,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return find_local_minima(cas::global_context(), f, vx, vy, x0, x1, y0, y1, opts); }

    static std::vector<CriticalPoint2D> find_local_maxima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts = {}
    );

    static std::vector<CriticalPoint2D> find_local_maxima(
        const cas::Expression& f, const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts = {}
    ) { return find_local_maxima(cas::global_context(), f, vx, vy, domain, opts); }

    static std::vector<CriticalPoint2D> find_local_maxima(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return find_local_maxima(ctx, f, vx, vy, Region::rectangle(x0, x1, y0, y1), opts); }

    static std::vector<CriticalPoint2D> find_local_maxima(
        const cas::Expression& f, const std::string& vx, const std::string& vy,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return find_local_maxima(cas::global_context(), f, vx, vy, x0, x1, y0, y1, opts); }

    static std::vector<CriticalPoint2D> find_saddle_points(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts = {}
    );

    static std::vector<CriticalPoint2D> find_saddle_points(
        const cas::Expression& f, const std::string& vx, const std::string& vy,
        const Region& domain, const SolverOptions2D& opts = {}
    ) { return find_saddle_points(cas::global_context(), f, vx, vy, domain, opts); }

    static std::vector<CriticalPoint2D> find_saddle_points(
        cas::SymbolicContext& ctx, const cas::Expression& f,
        const std::string& vx, const std::string& vy,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return find_saddle_points(ctx, f, vx, vy, Region::rectangle(x0, x1, y0, y1), opts); }

    static std::vector<CriticalPoint2D> find_saddle_points(
        const cas::Expression& f, const std::string& vx, const std::string& vy,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    ) { return find_saddle_points(cas::global_context(), f, vx, vy, x0, x1, y0, y1, opts); }

    static CriticalAnalysis2D analyze_critical_structure(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        const Region& domain,
        const SolverOptions2D& opts = {}
    );

    static CriticalAnalysis2D analyze_critical_structure(
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        const Region& domain,
        const SolverOptions2D& opts = {}
    ) { return analyze_critical_structure(cas::global_context(), f_expr, var_x, var_y, domain, opts); }

    static CriticalAnalysis2D analyze_critical_structure(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    );

    static CriticalAnalysis2D analyze_critical_structure(
        const cas::Expression& f_expr,
        const std::string& var_x, const std::string& var_y,
        double x0, double x1, double y0, double y1,
        const SolverOptions2D& opts = {}
    );

private:
    struct SystemPack {
        Fn2 f, g;
        Fn2 df_dx, df_dy;
        Fn2 dg_dx, dg_dy;
    };

    struct Cell {
        double x0, y0, x1, y1;
        double score;
        bool operator<(const Cell& o) const { return score < o.score; } 
    };

    enum class SliceVerdict { Min, Max, Neither, Inconclusive };

    static bool is_valid(double x) noexcept { return std::isfinite(x); }

    static double clamp_tol(double t) noexcept {
        return t > constants::type_epsilon() ? t : constants::type_epsilon();
    }

    static double finite_bound(double v, double fallback) noexcept {
        return std::isfinite(v) ? v : fallback;
    }

    static double combined_residual(double fv, double gv) noexcept;

    static bool is_duplicate(
        const std::vector<SolverResult2D>& results,
        double px, double py, double tol
    );

    static SystemPack make_system_lambdas(
        cas::SymbolicContext& ctx,
        const cas::Expression& f_expr, const cas::Expression& g_expr,
        const std::string& vx, const std::string& vy
    );

    static SolverResult2D newton2d(
        const SystemPack& sys,
        double x0, double y0,
        double tol, std::uint64_t max_iter
    );

    static double score_cell(
        const SystemPack& sys,
        double cx0, double cy0, double cx1, double cy1
    );

    static std::vector<SolverResult2D> solve_impl(
        const SystemPack& sys,
        const Region& domain,
        const SolverOptions2D& opts
    );

    static std::vector<CriticalPoint2D> filter_by_type(
        std::vector<CriticalPoint2D> pts,
        CriticalPointType type
    );

    static std::vector<CriticalPoint2D> classify_critical_points(
        cas::SymbolicContext& ctx,
        const cas::Expression& f,
        const cas::Expression& fx, const cas::Expression& fy, const cas::Expression& fxy,
        const std::string& varx, const std::string& vary,
        const std::vector<SolverResult2D>& solutions
    );

    static SliceVerdict classify_1d_slice(
        const Fn2& f, double fval,
        double px, double py,
        double dx, double dy,
        double h
    );

    static double probe_step(double px, double py) noexcept { return std::max(1e-6, std::max(std::abs(px), std::abs(py)) * 1e-5); }

    static CriticalPointType try_hessian(
        const Fn2& fxx, const Fn2& fyy, const Fn2& fxy,
        double px, double py
    );

    static CriticalPointType try_axis_slices(
        const Fn2& f, double fval,
        double px, double py
    );

    static CriticalPointType try_generic_lines(
        const Fn2& f, double fval,
        double px, double py
    );

    static CriticalPointType try_taylor(
        const Fn2& f,
        const Fn2& fxx, const Fn2& fyy, const Fn2& fxy,
        double fval, double px, double py
    );

    static CriticalPointType try_shrinking_circle(
        const Fn2& f, double fval,
        double px, double py
    );

    struct CoordGroup {
        double representative;       // median value of the cluster
        std::vector<std::size_t> indices;  // indices into the original points vector
    };

    static std::vector<CoordGroup> cluster_by_coordinate(
        const std::vector<CriticalPoint2D>& pts,
        bool by_x
    );

    static double snap_to_nice(double v, double tol = 1e-10) noexcept;

    static bool verify_critical_line(
        const cas::Expression& fx, const cas::Expression& fy,
        const std::string& var_x, const std::string& var_y,
        double fixed_val, bool x_is_fixed,
        const Region& domain
    );

    static CriticalCurve2D build_axis_line_curve(
        const std::string& fixed_var,
        const std::string& free_var,
        double fixed_val,
        const Interval& free_iv
    );

    static CriticalAnalysis2D detect_critical_curves(
        const cas::Expression& fx, const cas::Expression& fy,
        const std::string& var_x, const std::string& var_y,
        const Region& domain,
        const std::vector<CriticalPoint2D>& points
    );
    
    static void detect_axis_curves(
        const std::vector<CriticalPoint2D>& points,
        std::vector<bool>& consumed,
        const cas::Expression& fx, const cas::Expression& fy,
        const std::string& var_x, const std::string& var_y,
        const Region& domain,
        bool by_x,
        CriticalAnalysis2D& analysis
    );

    static std::vector<CriticalPoint2D> collapse_degenerate_clusters(
        const std::vector<CriticalPoint2D>& points
    );

    static void collapse_axis(
        const std::vector<CriticalPoint2D>& points,
        std::vector<bool>& consumed,
        bool by_x,
        std::vector<CriticalPoint2D>& result
    );
};

} // namespace solvers
} // namespace math
} // namespace fizmo

#endif // FIZMO_EQUATION_SOLVER_2D_HPP