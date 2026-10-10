#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace geometry {

FrenetFrame::FrenetFrame(vectors::SymbolicVector3 T, vectors::SymbolicVector3 N, vectors::SymbolicVector3 B, cas::Expression kappa, cas::Expression tau, std::string param) : T_(std::move(T)), N_(std::move(N)), B_(std::move(B))
        , kappa_(std::move(kappa)), tau_(std::move(tau))
        , param_(std::move(param))
    {}

OsculatingCircle::OsculatingCircle(vectors::SymbolicVector3 center, cas::Expression radius, vectors::SymbolicVector3 T, vectors::SymbolicVector3 N, std::string param) : center_(std::move(center)), radius_(std::move(radius))
        , T_(std::move(T)), N_(std::move(N))
        , param_(std::move(param))
    {}

OsculatingCircle2D::OsculatingCircle2D(vectors::SymbolicVector2 center, cas::Expression radius, vectors::SymbolicVector2 T, vectors::SymbolicVector2 N, std::string param) : center_(std::move(center)), radius_(std::move(radius)) , T_(std::move(T)), N_(std::move(N)), param_(std::move(param)) {}

OsculatingCircle4D::OsculatingCircle4D(vectors::SymbolicVector4 center, cas::Expression radius, vectors::SymbolicVector4 T, vectors::SymbolicVector4 N, std::string param) : center_(std::move(center)), radius_(std::move(radius)), T_(std::move(T)), N_(std::move(N)), param_(std::move(param)) {}

OsculatingCircleN::OsculatingCircleN(vectors::SymbolicVectorN center, cas::Expression radius, vectors::SymbolicVectorN T, vectors::SymbolicVectorN N, std::string param) : center_(std::move(center)), radius_(std::move(radius))
        , T_(std::move(T)), N_(std::move(N)), param_(std::move(param)) {}

auto ParametricCurve::r_prime() const -> const vectors::SymbolicVector3& {
    if (!rp_) rp_ = std::make_unique<vectors::SymbolicVector3>(r_.differentiate(param_));
    return *rp_;
}

auto ParametricCurve::r_double_prime() const -> const vectors::SymbolicVector3& {
    if (!rpp_) rpp_ = std::make_unique<vectors::SymbolicVector3>(r_prime().differentiate(param_));
    return *rpp_;
}

auto ParametricCurve::r_triple_prime() const -> const vectors::SymbolicVector3& {
    if (!rppp_) rppp_ = std::make_unique<vectors::SymbolicVector3>(r_double_prime().differentiate(param_));
    return *rppp_;
}

auto ParametricCurve::cross_rp_rpp() const -> const vectors::SymbolicVector3& {
    if (!cross_) cross_ = std::make_unique<vectors::SymbolicVector3>(r_prime().cross(r_double_prime()));
    return *cross_;
}

auto ParametricCurve::curvature() const -> cas::Expression {
    if (!curvature_) curvature_ = std::make_unique<cas::Expression>(
        cross_rp_rpp().magnitude() / (r_prime().magnitude_squared() * r_prime().magnitude())
    );
    return *curvature_;
}

auto ParametricCurve::torsion() const -> cas::Expression {
    if (!torsion_) torsion_ = std::make_unique<cas::Expression>(
        cross_rp_rpp().dot(r_triple_prime()) / cross_rp_rpp().magnitude_squared()
    );
    return *torsion_;
}

auto ParametricCurve::B() const -> const vectors::SymbolicVector3& {
    if (!B_) B_ = std::make_unique<vectors::SymbolicVector3>(cross_rp_rpp().normalized());
    return *B_;
}

auto ParametricCurve::inv_curvature() const -> const cas::Expression& {
    if (!inv_curvature_) inv_curvature_ = std::make_unique<cas::Expression>(cas::Const(1.0) / curvature());
    return *inv_curvature_;
}

auto ParametricCurve::osculating_center() const -> const vectors::SymbolicVector3& {
    if (!osculating_center_) osculating_center_ = std::make_unique<vectors::SymbolicVector3>(r_ + N() * inv_curvature());
    return *osculating_center_;
}

auto ParametricCurve2D::r_prime() const -> const vectors::SymbolicVector2& {
    if (!rp_) rp_ = std::make_unique<vectors::SymbolicVector2>(r_.differentiate(param_));
    return *rp_;
}

auto ParametricCurve2D::r_double_prime() const -> const vectors::SymbolicVector2& {
    if (!rpp_) rpp_ = std::make_unique<vectors::SymbolicVector2>(r_prime().differentiate(param_));
    return *rpp_;
}

auto ParametricCurve2D::r_triple_prime() const -> const vectors::SymbolicVector2& {
    if (!rppp_) rppp_ = std::make_unique<vectors::SymbolicVector2>(r_double_prime().differentiate(param_));
    return *rppp_;
}

auto ParametricCurve2D::cross_2d() const -> const cas::Expression& {
    if (!cross2d_) {
        auto& rp  = r_prime();
        auto& rpp = r_double_prime();
        cross2d_ = std::make_unique<cas::Expression>(rp.x * rpp.y - rp.y * rpp.x);
    }
    return *cross2d_;
}

auto ParametricCurve2D::signed_curvature() const -> cas::Expression {
    if (!signed_curvature_) {
        cas::Expression sp3 = r_prime().magnitude_squared() * r_prime().magnitude();
        signed_curvature_ = std::make_unique<cas::Expression>(cross_2d() / sp3);
    }
    return *signed_curvature_;
}

auto ParametricCurve2D::curvature() const -> cas::Expression {
    if (!curvature_) {
        cas::Expression sp3 = r_prime().magnitude_squared() * r_prime().magnitude();
        curvature_ = std::make_unique<cas::Expression>(ABS(cross_2d()) / sp3);
    }
    return *curvature_;
}

auto ParametricCurve2D::N() const -> const vectors::SymbolicVector2& {
    if (!N_) {
        auto& t = T();
        N_ = std::make_unique<vectors::SymbolicVector2>(vectors::SymbolicVector2{ -t.y, t.x });
    }
    return *N_;
}

auto ParametricCurve2D::inv_curvature() const -> const cas::Expression& {
    if (!inv_curvature_) inv_curvature_ = std::make_unique<cas::Expression>(cas::Const(1.0) / curvature());
    return *inv_curvature_;
}

auto ParametricCurve2D::osculating_center() const -> const vectors::SymbolicVector2& {
    if (!osc_center_) osc_center_ = std::make_unique<vectors::SymbolicVector2>(r_ + N() * inv_curvature());
    return *osc_center_;
}

bool ParametricCurve2D::is_closed(double t0, double t1, double tol) const {
    tol = std::max(tol, constants::type_epsilon());
    auto p0 = position_at(t0);
    auto p1 = position_at(t1);
    return dist_squared(p0, p1) < tol * tol;
}

auto ParametricCurve2D::find_closure(
    double t0, double t1,
    std::uint64_t n_samples,
    double tol 
) const -> ClosureResult {
    tol = std::max(tol, constants::type_epsilon());
    const double tol_sq = tol * tol;
    const auto p0 = position_at(t0);
    const double dt = (t1 - t0) / static_cast<double>(n_samples);
    double pprev_t = t0 + dt;
    double pprev_g = dist_squared(p0, position_at(pprev_t));
    double prev_t  = pprev_t;
    double prev_g  = pprev_g;

    for (std::uint64_t i = 2; i <= n_samples; ++i) {
        double cur_t = t0 + i * dt;
        double cur_g = dist_squared(p0, position_at(cur_t));
        if (prev_g < tol_sq) { return { true, prev_t, position_at(prev_t) }; }

        if (i >= 3 && pprev_g > prev_g && cur_g >= prev_g) {
            auto result = refine_closure(p0, pprev_t, cur_t, tol_sq);
            if (result.found) return result;
        }

        pprev_t = prev_t;
        pprev_g = prev_g;
        prev_t  = cur_t;
        prev_g  = cur_g;
    }

    if (dist_squared(p0, position_at(t1)) < tol_sq) { return { true, t1, position_at(t1) }; }

    return {};
}

bool ParametricCurve2D::has_closure(
    double t0, double t1,
    std::uint64_t n_samples,
    double tol 
) const {
    tol = std::max(tol, constants::type_epsilon());
    if (is_closed(t0, t1, tol)) return true;
    return find_closure(t0, t1, n_samples, tol).found;
}

auto ParametricCurve2D::refine_closure(
    const Vector2D<double, double>& p0,
    double a, double b,
    double tol_sq
) const -> ClosureResult {
    constexpr double resphi = 2.0 - constants::phi();  
    constexpr std::uint64_t max_iter = 128;

    for (std::uint64_t i = 0; i < max_iter; ++i) {
        if (std::abs(b - a) < constants::type_epsilon()) break;
        double m1 = a + resphi * (b - a);
        double m2 = b - resphi * (b - a);

        if (dist_squared(p0, position_at(m1)) < dist_squared(p0, position_at(m2))) {
            b = m2;
        } else {
            a = m1;
        }
    }

    double t_min = 0.5 * (a + b);
    auto   p_min = position_at(t_min);
    if (dist_squared(p0, p_min) < tol_sq) { return { true, t_min, p_min }; }
    return {};
}

ParametricCurve4D::ParametricCurve4D(cas::Expression x_t, cas::Expression y_t, cas::Expression z_t, cas::Expression w_t, std::string param) : r_(std::move(x_t), std::move(y_t), std::move(z_t), std::move(w_t)), param_(std::move(param)) {}

auto ParametricCurve4D::r_prime() const -> const vectors::SymbolicVector4& {
    if (!rp_) rp_ = std::make_unique<vectors::SymbolicVector4>(r_.differentiate(param_));
    return *rp_;
}

auto ParametricCurve4D::r_double_prime() const -> const vectors::SymbolicVector4& {
    if (!rpp_) rpp_ = std::make_unique<vectors::SymbolicVector4>(r_prime().differentiate(param_));
    return *rpp_;
}

auto ParametricCurve4D::r_triple_prime() const -> const vectors::SymbolicVector4& {
    if (!rppp_) rppp_ = std::make_unique<vectors::SymbolicVector4>(r_double_prime().differentiate(param_));
    return *rppp_;
}

auto ParametricCurve4D::curvature() const -> cas::Expression {
    if (!curvature_) {
        auto& rp  = r_prime();
        auto& rpp = r_double_prime();
        cas::Expression rp_sq  = rp.magnitude_squared();
        cas::Expression rpp_sq = rpp.magnitude_squared();
        cas::Expression dot_rp_rpp = rp.dot(rpp);
        cas::Expression numer = SQRT(rp_sq * rpp_sq - dot_rp_rpp * dot_rp_rpp);
        cas::Expression denom = rp_sq * rp.magnitude();
        curvature_ = std::make_unique<cas::Expression>(numer / denom);
    }
    return *curvature_;
}

auto ParametricCurve4D::N() const -> const vectors::SymbolicVector4& {
    if (!N_) {
        auto& t   = T();
        auto& rpp = r_double_prime();
        cas::Expression proj = rpp.dot(t);
        vectors::SymbolicVector4 perp = rpp - t * proj;
        N_ = std::make_unique<vectors::SymbolicVector4>(perp.normalized());
    }
    return *N_;
}

auto ParametricCurve4D::inv_curvature() const -> const cas::Expression& {
    if (!inv_curvature_) inv_curvature_ = std::make_unique<cas::Expression>(cas::Const(1.0) / curvature());
    return *inv_curvature_;
}

auto ParametricCurve4D::osculating_center() const -> const vectors::SymbolicVector4& {
    if (!osc_center_) osc_center_ = std::make_unique<vectors::SymbolicVector4>(r_ + N() * inv_curvature());
    return *osc_center_;
}

auto ParametricCurveN::r_prime() const -> const vectors::SymbolicVectorN& {
    if (!rp_) rp_ = std::make_unique<vectors::SymbolicVectorN>(r_.differentiate(param_));
    return *rp_;
}

auto ParametricCurveN::r_double_prime() const -> const vectors::SymbolicVectorN& {
    if (!rpp_) rpp_ = std::make_unique<vectors::SymbolicVectorN>(r_prime().differentiate(param_));
    return *rpp_;
}

auto ParametricCurveN::r_triple_prime() const -> const vectors::SymbolicVectorN& {
    if (!rppp_) rppp_ = std::make_unique<vectors::SymbolicVectorN>(r_double_prime().differentiate(param_));
    return *rppp_;
}

auto ParametricCurveN::curvature() const -> cas::Expression {
    if (!curvature_) {
        auto& rp  = r_prime();
        auto& rpp = r_double_prime();
        cas::Expression rp_sq  = rp.magnitude_squared();
        cas::Expression rpp_sq = rpp.magnitude_squared();
        cas::Expression dot_rp_rpp = rp.dot(rpp);
        cas::Expression numer = SQRT(rp_sq * rpp_sq - dot_rp_rpp * dot_rp_rpp);
        cas::Expression denom = rp_sq * rp.magnitude(); 
        curvature_ = std::make_unique<cas::Expression>(numer / denom);
    }
    return *curvature_;
}

auto ParametricCurveN::N() const -> const vectors::SymbolicVectorN& {
    if (!N_) {
        auto& t   = T();
        auto& rpp = r_double_prime();
        cas::Expression proj = rpp.dot(t);        
        vectors::SymbolicVectorN perp = rpp - t * proj; 
        N_ = std::make_unique<vectors::SymbolicVectorN>(perp.normalized());
    }
    return *N_;
}

auto ParametricCurveN::inv_curvature() const -> const cas::Expression& {
    if (!inv_curvature_) inv_curvature_ = std::make_unique<cas::Expression>(cas::Const(1.0) / curvature());
    return *inv_curvature_;
}

auto ParametricCurveN::osculating_center() const -> const vectors::SymbolicVectorN& {
    if (!osc_center_) osc_center_ = std::make_unique<vectors::SymbolicVectorN>(r_ + N() * inv_curvature());
    return *osc_center_;
}

} // namespace geometry
} // namespace math
} // namespace fizmo
