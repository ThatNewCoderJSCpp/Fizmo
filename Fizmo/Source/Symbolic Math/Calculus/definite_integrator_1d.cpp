#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace integration {

auto BasicIntegration::integrate_simpson(
    std::function<double(double)> f, const Interval& iv,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    if (iv.lower.value > iv.upper.value) {
        IntegrationResult switched = integrate_simpson(f, Interval(iv.upper, iv.lower), config, coarse_epsilon, fine_epsilon);
        switched.value = -switched.value;
        switched.method = IntegrationTechnique::Simpson;
        return switched;
    }

    if (std::abs(iv.lower.value - iv.upper.value) <= constants::middle_epsilon()) {
        IntegrationResult ir;
        ir.converged = true;
        ir.method = IntegrationTechnique::Simpson;
        return ir;
    }

    if (config.transform == InfiniteTransform::Auto) { config.transform = resolve_infinite_transform(iv, IntegrationTechnique::Simpson); }
    clamp_value(coarse_epsilon, constants::type_epsilon(), 1.0);
    clamp_value(fine_epsilon, constants::type_epsilon(), 1.0);
    auto transformed_f = apply_infinite_transform(std::move(f), iv, config.transform);
    const double a = transformed_f.a;
    const double b = transformed_f.b;
    IntegrationResult result;

    switch (config.strategy) {
        case AdaptiveStrategy::Local:  result = run_local_simpson(transformed_f.g, a, b, config); break;
        case AdaptiveStrategy::Global: result = global_adaptive_simpson(transformed_f.g, a, b, config); break;

        case AdaptiveStrategy::Hybrid:
        default:
            result = hybrid_adaptive_simpson(transformed_f.g, a, b, config); 
            break;
    }

    result.method = IntegrationTechnique::Simpson;
    return result;
}

auto BasicIntegration::integrate_simpson(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_simpson(std::move(f), Interval(Endpoint(a, true), Endpoint(b, true)), config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_gauss_legendre(
    std::function<double(double)> f, const Interval& iv,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    if (iv.lower.value > iv.upper.value) {
        IntegrationResult switched = integrate_gauss_legendre(f, Interval(iv.upper, iv.lower), config, coarse_epsilon, fine_epsilon);
        switched.value = -switched.value;
        switched.method = IntegrationTechnique::GaussLegendre;
        return switched;
    }

    if (std::abs(iv.lower.value - iv.upper.value) <= constants::middle_epsilon()) {
        IntegrationResult ir;
        ir.converged = true;
        ir.method = IntegrationTechnique::GaussLegendre;
        return ir;
    }

    if (config.transform == InfiniteTransform::Auto) { config.transform = resolve_infinite_transform(iv, IntegrationTechnique::GaussLegendre); }
    clamp_value(coarse_epsilon, constants::type_epsilon(), 1.0);
    clamp_value(fine_epsilon, constants::type_epsilon(), 1.0);
    auto transformed_f = apply_infinite_transform(std::move(f), iv, config.transform);
    auto result = gauss_legendre_core(transformed_f.g, transformed_f.a, transformed_f.b);
    result.method = IntegrationTechnique::GaussLegendre;
    return result;
}

auto BasicIntegration::integrate_gauss_legendre(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_gauss_legendre(std::move(f), Interval(Endpoint(a, true), Endpoint(b, true)), config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_gauss_kronrod(
    std::function<double(double)> f, const Interval& iv,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    if (iv.lower.value > iv.upper.value) {
        IntegrationResult switched = integrate_gauss_kronrod(f, Interval(iv.upper, iv.lower), config, coarse_epsilon, fine_epsilon);
        switched.value = -switched.value;
        switched.method = IntegrationTechnique::GaussKronrod;
        return switched;
    }

    if (std::abs(iv.lower.value - iv.upper.value) <= constants::middle_epsilon()) {
        IntegrationResult ir;
        ir.converged = true;
        ir.method = IntegrationTechnique::GaussKronrod;
        return ir;
    }

    if (config.transform == InfiniteTransform::Auto) {
        config.transform = resolve_infinite_transform(iv, IntegrationTechnique::GaussKronrod);
    }

    clamp_value(coarse_epsilon, constants::type_epsilon(), 1.0);
    clamp_value(fine_epsilon, constants::type_epsilon(), 1.0);
    auto transformed_f = apply_infinite_transform(std::move(f), iv, config.transform);

    IntegrationResult result = adaptive_gauss_kronrod(
        transformed_f.g,
        transformed_f.a, transformed_f.b,
        config
    );

    result.method = IntegrationTechnique::GaussKronrod;
    return result;
}

auto BasicIntegration::integrate_gauss_kronrod(
    std::function<double(double)> f, double a, double b,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    return integrate_gauss_kronrod(
        std::move(f),
        Interval(Endpoint(a, true), Endpoint(b, true)),
        config, coarse_epsilon, fine_epsilon
    );
}

auto BasicIntegration::integrate_trapezoidal(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_simple_rule(std::move(f), iv, IntegrationTechnique::Trapezoidial, config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_trapezoidal(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_trapezoidal(std::move(f), Interval(Endpoint(a, true), Endpoint(b, true)), config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_left_rect(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_simple_rule(std::move(f), iv, IntegrationTechnique::LeftRect, config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_left_rect(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_left_rect(std::move(f), Interval(Endpoint(a, true), Endpoint(b, true)), config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_right_rect(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_simple_rule(std::move(f), iv, IntegrationTechnique::RightRect, config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_right_rect(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_right_rect(std::move(f), Interval(Endpoint(a, true), Endpoint(b, true)), config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_midpoint_rect(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_simple_rule(std::move(f), iv, IntegrationTechnique::MidpointRect, config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_midpoint_rect(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config,
        double coarse_epsilon,
        double fine_epsilon   
) -> IntegrationResult { return integrate_midpoint_rect(std::move(f), Interval(Endpoint(a, true), Endpoint(b, true)), config, coarse_epsilon, fine_epsilon); }

auto BasicIntegration::integrate_romberg(
    std::function<double(double)> f, const Interval& iv,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    auto result = integrate_trapezoidal(std::move(f), iv, config, coarse_epsilon, fine_epsilon);
    result.method = IntegrationTechnique::Romberg;
    return result;
}

auto BasicIntegration::integrate_romberg(
    std::function<double(double)> f, double a, double b,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    auto result = integrate_trapezoidal(std::move(f), a, b, config, coarse_epsilon, fine_epsilon);
    result.method = IntegrationTechnique::Romberg;
    return result;
}

auto BasicIntegration::integrate_clenshaw_curtis(
    std::function<double(double)> f, const Interval& iv,
    std::size_t n,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    n = std::max<std::size_t>(n, 3);
    auto nw = cc_nodes_weights(n);
    return cc_integrate_preamble(std::move(f), iv, config, coarse_epsilon, fine_epsilon, nw.data(), nw.size());
}

auto BasicIntegration::integrate_clenshaw_curtis(
    std::function<double(double)> f, double a, double b,
    std::size_t n,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    return integrate_clenshaw_curtis(std::move(f), Interval(Endpoint(a, true), Endpoint(b, true)), n, config, coarse_epsilon, fine_epsilon);
}

auto BasicIntegration::integrate_gauss_legendre(
    std::function<double(double)> f, const Interval& iv,
    std::size_t n,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    n = std::max<std::size_t>(n, 1);
    auto nw = gl_nodes_weights(n);
    return gl_integrate_preamble(
        std::move(f), iv, config,
        coarse_epsilon, fine_epsilon, nw.data(), nw.size()
    );
}

auto BasicIntegration::integrate_gauss_legendre(
    std::function<double(double)> f, double a, double b,
    std::size_t n,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    return integrate_gauss_legendre(
        std::move(f),
        Interval(Endpoint(a, true), Endpoint(b, true)),
        n, config, coarse_epsilon, fine_epsilon
    );
}

auto BasicIntegration::integrate_tanh_sinh(
    std::function<double(double)> f, const Interval& iv,
    std::size_t n,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    n = std::max<std::size_t>(n, 1);
    auto nw = ts_nodes_weights(n);

    return ts_integrate_preamble(
        std::move(f), iv, config,
        coarse_epsilon, fine_epsilon, nw.data(), nw.size()
    );
}

auto BasicIntegration::integrate_tanh_sinh(
    std::function<double(double)> f, double a, double b,
    std::size_t n,
    IntegrationConfig config,
    double coarse_epsilon,
    double fine_epsilon   
) -> IntegrationResult {
    return integrate_tanh_sinh(
        std::move(f),
        Interval(Endpoint(a, true), Endpoint(b, true)),
        n, config, coarse_epsilon, fine_epsilon
    );
}

BasicIntegration::SubInterval::SubInterval(double a_, double b_, const std::function<double(double)>& f) : a(a_), b(b_) {
    fa = f(a);
    fb = f(b);
    fm = f(0.5 * (a + b));
    S  = BasicIntegration::simpson_rule((b - a), fa, fm, fb);
    double m1 = 0.5 * (a + (a + b) * 0.5);
    double m2 = 0.5 * ((a + b) * 0.5 + b);
    double f1 = f(m1);
    double f2 = f(m2);
    double S_left  = BasicIntegration::simpson_rule((0.5 * (b - a)), fa, f1, fm);
    double S_right = BasicIntegration::simpson_rule((0.5 * (b - a)), fm, f2, fb);
    S2  = S_left + S_right;
    err = std::abs(S2 - S);
}

BasicIntegration::SimpleRuleSubInterval::SimpleRuleSubInterval(double a_, double b_, const std::function<double(double)>& f, IntegrationTechnique rule_) : a(a_), b(b_), rule(rule_) {
    const double h = b - a;
    fa = f(a);
    fb = f(b);
    fm = f(0.5 * (a + b));
    S = simple_panel(rule, h, fa, fm, fb);
    const double mid = 0.5 * (a + b);
    const double h2  = 0.5 * h;
    double f_m1 = f(0.5 * (a + mid));    
    double f_m2 = f(0.5 * (mid + b));    
    double S_left  = simple_panel(rule, h2, fa, f_m1, fm);
    double S_right = simple_panel(rule, h2, fm, f_m2, fb);
    S2  = S_left + S_right;
    err = std::abs(S2 - S);
}

auto BasicIntegration::SimpleRuleSubInterval::split(const std::function<double(double)>& f) const -> std::pair<SimpleRuleSubInterval, SimpleRuleSubInterval> {
    double m = 0.5 * (a + b);
    return { SimpleRuleSubInterval(a, m, f, rule), SimpleRuleSubInterval(m, b, f, rule) };
}

auto BasicIntegration::resolve_infinite_transform(BoundType bound, IntegrationTechnique technique) noexcept -> InfiniteTransform {
    if (bound == BoundType::Finite) return InfiniteTransform::Rational;
    if (technique == IntegrationTechnique::TanhSinh) return InfiniteTransform::TanhSinh;

    if (bound == BoundType::FullyInfinite) {
        switch (technique) {
            case IntegrationTechnique::GaussLegendre:
            case IntegrationTechnique::GaussKronrod:
            case IntegrationTechnique::ClenshawCurtis:
            case IntegrationTechnique::Romberg:
            case IntegrationTechnique::Trapezoidial:
                return InfiniteTransform::TanhSinh;

            default:
                return InfiniteTransform::LogRational;
        }
    }

    switch (technique) {
        case IntegrationTechnique::GaussLegendre:
        case IntegrationTechnique::GaussKronrod:
        case IntegrationTechnique::ClenshawCurtis:
            return InfiniteTransform::TanhSinh;

        case IntegrationTechnique::Romberg:
        case IntegrationTechnique::Trapezoidial:
            return InfiniteTransform::Exponential;

        case IntegrationTechnique::Simpson:
        case IntegrationTechnique::LeftRect:
        case IntegrationTechnique::MidpointRect:
        case IntegrationTechnique::RightRect:
            return InfiniteTransform::Rational;

        default:
            return InfiniteTransform::Rational;
    }
}

auto BasicIntegration::interval_to_bound_type(const Interval& iv) noexcept -> BoundType {
    const bool li = iv.lower.is_infinite();
    const bool ui = iv.upper.is_infinite();
    if (li && ui)  return BoundType::FullyInfinite;
    if (li)        return BoundType::LeftInfinite;
    if (ui)        return BoundType::RightInfinite;
    return BoundType::Finite;
}

auto BasicIntegration::apply_infinite_transform(std::function<double(double)> f, const Interval& iv, InfiniteTransform transform) -> TransformedIntegrand {
    const BoundType bound = interval_to_bound_type(iv);
    if (bound == BoundType::Finite) return { std::move(f), iv.lower.value, iv.upper.value };

    if (bound == BoundType::LeftInfinite) {
        const double b = iv.upper.value;
        auto flipped = [f = std::move(f), b](double u) { return f(b - u); };
        return apply_right_infinite(std::move(flipped), 0.0, transform);
    }

    if (bound == BoundType::RightInfinite) return apply_right_infinite(std::move(f), iv.lower.value, transform);
    return apply_fully_infinite(std::move(f), transform);
}

auto BasicIntegration::apply_right_infinite(std::function<double(double)> f, double a, InfiniteTransform transform) -> TransformedIntegrand {
    switch (transform) {
        case InfiniteTransform::Rational: {
            return {
                [f = std::move(f), a](double t) {
                    const double omt = 1.0 - t;
                    return f(a + t / omt) / (omt * omt);
                },
                0.0, 1.0 - constants::middle_epsilon()
            };
        }

        case InfiniteTransform::Exponential: {
            return {
                [f = std::move(f), a](double t) {
                    const double omt = 1.0 - t;
                    return f(a - std::log(omt)) / omt;
                },
                0.0, 1.0 - constants::middle_epsilon()
            };
        }

        case InfiniteTransform::TanhSinh: {
            constexpr double pi_2 = constants::pi_2();
            return {
                [f = std::move(f), a, pi_2](double t) {
                    const double sh = std::sinh(t);
                    const double ch = std::cosh(t);
                    const double ex = std::exp(pi_2 * sh);
                    return f(a + ex) * ex * pi_2 * ch;
                },
                -DE_TRUNC, DE_TRUNC
            };
        }

        default: return apply_right_infinite(std::move(f), a, InfiniteTransform::Rational);
    }
}

auto BasicIntegration::apply_fully_infinite(std::function<double(double)> f, InfiniteTransform transform) -> TransformedIntegrand {
    switch (transform) {
        case InfiniteTransform::LogRational: {
            return {
                [f = std::move(f)](double t) {
                    const double omt = 1.0 - t;
                    return f(std::log(t / omt)) / (t * omt);
                },
                constants::middle_epsilon(), 1.0 - constants::middle_epsilon()
            };
        }

        case InfiniteTransform::Tangent: {
            return {
                [f = std::move(f)](double t) {
                    const double arg = constants::pi() * (t - 0.5);
                    const double c   = std::cos(arg);
                    return f(std::tan(arg)) * constants::pi() / (c * c);
                },
                constants::middle_epsilon(), 1.0 - constants::middle_epsilon()
            };
        }

        case InfiniteTransform::TanhSinh: {
            constexpr double pi_2 = constants::pi_2();
            return {
                [f = std::move(f), pi_2](double t) {
                    const double sh    = std::sinh(t);
                    const double ch    = std::cosh(t);
                    const double inner = pi_2 * sh;
                    return f(std::sinh(inner)) * std::cosh(inner) * pi_2 * ch;
                },
                -DE_TRUNC, DE_TRUNC
            };
        }

        case InfiniteTransform::Rational: {
            return {
                [f = std::move(f)](double t) {
                    const double t2 = t * t;
                    const double omt2 = 1.0 - t2;
                    const double opt2 = 1.0 + t2;
                    return f(t / omt2) * opt2 / (omt2 * omt2);
                },
                constants::middle_epsilon() - 1.0,
                1.0 + constants::middle_epsilon()
            };
        }

        default: return apply_fully_infinite(std::move(f), InfiniteTransform::LogRational);
    }
}

double BasicIntegration::local_adaptive_simpson(
    const std::function<double(double)>& f, double a, double b,
    double fa, double fm, double fb,
    double S, double tol, int depth,
    IntegrationResult& ir
) {
    double m  = 0.5 * (a + b);
    double m1 = 0.5 * (a + m);
    double m2 = 0.5 * (m + b);
    double f1 = f(m1);
    double f2 = f(m2);
    ir.function_evaluations += 2;
    double S_left  = simpson_rule((m - a), fa, f1, fm);
    double S_right = simpson_rule((b - m), fm, f2, fb);
    double S2 = S_left + S_right;
    if (depth <= 0 || std::abs(S2 - S) < 15.0 * tol) { return S2 + (S2 - S) / 15.0; }
    return local_adaptive_simpson(f, a, m, fa, f1, fm, S_left, 0.5 * tol, depth - 1, ir) + local_adaptive_simpson(f, m, b, fm, f2, fb, S_right, 0.5 * tol, depth - 1, ir);
}

auto BasicIntegration::run_local_simpson(const std::function<double(double)>& f, double a, double b, const IntegrationConfig& cfg) -> IntegrationResult {
    IntegrationResult ir;
    double fa = f(a);
    double fb = f(b);
    double fm = f(0.5 * (a + b));
    ir.function_evaluations += 3;
    double S = simpson_rule((b - a), fa, fm, fb);

    double val = local_adaptive_simpson(
        f, a, b,
        fa, fm, fb,
        S,
        cfg.tolerance,
        cfg.max_depth,
        ir
    );

    ir.value = val;
    ir.error_estimate = 0.0;
    ir.converged = true;
    return ir;
}

auto BasicIntegration::global_adaptive_simpson(const std::function<double(double)>& f, double a, double b, const IntegrationConfig& cfg) -> IntegrationResult {
    IntegrationResult ir;
    std::priority_queue<SubInterval> pq;
    pq.emplace(a, b, f);
    ir.function_evaluations += 3;
    double total = 0.0;
    double total_comp = 0.0;
    double total_err = 0.0;
    double err_comp = 0.0;

    while (!pq.empty() && pq.size() < cfg.max_subdivisions) {
        SubInterval top = pq.top();
        pq.pop();

        if (top.err < cfg.tolerance) {
            double y = top.S2 - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = top.err - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
            continue;
        }

        auto [L, R] = top.split(f);
        ir.function_evaluations += 4;
        pq.push(L);
        pq.push(R);
    }

    ir.value = total;
    ir.error_estimate = total_err;
    ir.converged = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::hybrid_adaptive_simpson(const std::function<double(double)>& f, double a, double b, const IntegrationConfig& cfg) -> IntegrationResult {
    std::priority_queue<SubInterval> pq;
    pq.emplace(a, b, f);
    IntegrationResult ir;
    ir.function_evaluations += 3;

    for (std::uint64_t i = 0; i < cfg.global_levels; ++i) {
        if (pq.empty()) break;
        SubInterval top = pq.top();
        pq.pop();
        auto [L, R] = top.split(f);
        ir.function_evaluations += 4;
        pq.push(L);
        pq.push(R);
    }

    double total = 0.0;
    double total_comp = 0.0;
    double total_err = 0.0;
    double err_comp = 0.0;

    while (!pq.empty()) {
        SubInterval s = pq.top();
        pq.pop();

        double val = local_adaptive_simpson(
            f, s.a, s.b,
            s.fa, s.fm, s.fb,
            s.S,
            cfg.tolerance,
            cfg.max_depth,
            ir
        );

        double y = val - total_comp;
        double t = total + y;
        total_comp = (t - total) - y;
        total = t;
        double ye = s.err - err_comp;
        double te = total_err + ye;
        err_comp = (te - total_err) - ye;
        total_err = te;
    }

    ir.value = total;
    ir.error_estimate = total_err;
    ir.converged = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::gauss_legendre_core(const std::function<double(double)>& f, double a, double b) -> IntegrationResult {
    IntegrationResult ir;
    const double mid = (b + a) * 0.5;
    const double half_length = (b - a) * 0.5;
    double sum = 0.0;
    double comp = 0.0;

    for (unsigned int i = 0; i < GL_P; ++i) {
        double y = gl_weights[i] * f(mid + half_length * gl_nodes[i]) - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum = t;
    }

    ir.value = half_length * sum;
    ir.function_evaluations = GL_P;
    ir.converged = true;
    return ir;
}

auto BasicIntegration::gauss_kronrod_panel(
    const std::function<double(double)>& f,
    double a, double b
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = IntegrationTechnique::GaussKronrod;
    const double mid         = (a + b) * 0.5;
    const double half_length = (b - a) * 0.5;
    double fv[GK_KS];

    for (unsigned int i = 0; i < GK_KS; ++i) {
        fv[i] = f(mid + half_length * gk_kronrod_nodes[i]);

        if (!std::isfinite(fv[i])) {
            ir.value          = constants::quiet_nan();
            ir.error_estimate = constants::positive_infinity();
            ir.converged      = false;
            ir.function_evaluations = i + 1;
            return ir;
        }
    }

    ir.function_evaluations = GK_KS;
    double k_sum  = 0.0;
    double k_comp = 0.0;

    for (unsigned int i = 0; i < GK_KS; ++i) {
        double y = gk_kronrod_weights[i] * fv[i] - k_comp;
        double t = k_sum + y;
        k_comp   = (t - k_sum) - y;
        k_sum    = t;
    }

    k_sum *= half_length;
    double g_sum  = 0.0;
    double g_comp = 0.0;

    for (unsigned int i = 0; i < GK_GS; ++i) {
        double y = gk_gauss_weights[i] * fv[gk_gauss_indices[i]] - g_comp;
        double t = g_sum + y;
        g_comp   = (t - g_sum) - y;
        g_sum    = t;
    }

    g_sum *= half_length;
    ir.value          = k_sum;
    ir.error_estimate = std::abs(k_sum - g_sum);
    ir.converged      = true;   
    return ir;
}

auto BasicIntegration::adaptive_gauss_kronrod_recursive(
    const std::function<double(double)>& f,
    double a, double b,
    double tolerance,
    std::uint64_t depth, std::uint64_t max_depth,
    std::uint64_t& total_evals
) -> IntegrationResult {
    auto panel = gauss_kronrod_panel(f, a, b);
    total_evals += panel.function_evaluations;

    if (panel.error_estimate <= tolerance || depth >= max_depth || !std::isfinite(panel.error_estimate)) {
        panel.converged = (panel.error_estimate <= tolerance);
        return panel;
    }

    const double mid     = (a + b) * 0.5;
    const double h_left  = mid - a;
    const double h_right = b - mid;
    const double span    = b - a;
    const double tol_l   = tolerance * h_left  / span;
    const double tol_r   = tolerance * h_right / span;
    auto left  = adaptive_gauss_kronrod_recursive(f, a, mid, tol_l, depth + 1, max_depth, total_evals);
    auto right = adaptive_gauss_kronrod_recursive(f, mid, b, tol_r, depth + 1, max_depth, total_evals);
    IntegrationResult combined;
    combined.value               = left.value + right.value;
    combined.error_estimate      = left.error_estimate + right.error_estimate;
    combined.converged           = left.converged && right.converged;
    combined.function_evaluations = 0;  
    combined.method              = IntegrationTechnique::GaussKronrod;
    return combined;
}

auto BasicIntegration::adaptive_gauss_kronrod(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg
) -> IntegrationResult {
    std::uint64_t total_evals = 0;

    auto result = adaptive_gauss_kronrod_recursive(
        f, a, b,
        cfg.tolerance,
        0, cfg.max_depth,
        total_evals
    );

    result.function_evaluations = total_evals;
    result.method = IntegrationTechnique::GaussKronrod;
    return result;
}

double BasicIntegration::simple_panel(IntegrationTechnique rule, double h, double fa, double fm, double fb) noexcept {
    switch (rule) {
        case IntegrationTechnique::Trapezoidial: return trapezoid_rule(h, fa, fb);
        case IntegrationTechnique::LeftRect:     return left_rect_rule(h, fa);
        case IntegrationTechnique::RightRect:    return right_rect_rule(h, fb);
        case IntegrationTechnique::MidpointRect: return midpoint_rule(h, fm);
        default: return 0.0;
    }
}

auto BasicIntegration::integrate_simple_rule(
    std::function<double(double)> f, const Interval& iv,
    IntegrationTechnique rule,
    IntegrationConfig config,
    double coarse_epsilon, double fine_epsilon
) -> IntegrationResult {
    if (iv.lower.value > iv.upper.value) {
        IntegrationResult switched = integrate_simple_rule(f, Interval(iv.upper, iv.lower), rule, config, coarse_epsilon, fine_epsilon);
        switched.value = -switched.value;
        switched.method = rule;
        return switched;
    }

    if (std::abs(iv.lower.value - iv.upper.value) <= constants::middle_epsilon()) {
        IntegrationResult ir;
        ir.converged = true;
        ir.method = rule;
        return ir;
    }

    if (config.transform == InfiniteTransform::Auto) {
        config.transform = resolve_infinite_transform(iv, rule);
    }

    clamp_value(coarse_epsilon, constants::type_epsilon(), 1.0);
    clamp_value(fine_epsilon, constants::type_epsilon(), 1.0);
    auto transformed_f = apply_infinite_transform(std::move(f), iv, config.transform);
    const double a = transformed_f.a;
    const double b = transformed_f.b;
    IntegrationResult result;

    switch (config.strategy) {
        case AdaptiveStrategy::Local:
            result = run_local_simple(transformed_f.g, a, b, config, rule);
            break;
        case AdaptiveStrategy::Global:
            result = global_adaptive_simple(transformed_f.g, a, b, config, rule);
            break;
        case AdaptiveStrategy::Hybrid:
        default:
            result = hybrid_adaptive_simple(transformed_f.g, a, b, config, rule);
            break;
    }

    result.method = rule;
    return result;
}

auto BasicIntegration::run_local_simple(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    IntegrationTechnique rule
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = rule;
    const std::uint64_t N = (cfg.max_subdivisions > 0) ? cfg.max_subdivisions : 10000;
    const double h = (b - a) / static_cast<double>(N);
    double sum  = 0.0;
    double comp = 0.0;

    switch (rule) {
        case IntegrationTechnique::Trapezoidial: {
            sum = 0.5 * f(a);

            for (std::uint64_t i = 1; i < N; ++i) {
                double y = f(a + i * h) - comp;
                double t = sum + y;
                comp = (t - sum) - y;
                sum  = t;
            }

            double y = 0.5 * f(b) - comp;
            double t = sum + y;
            sum = t;
            ir.value = h * sum;
            ir.function_evaluations = N + 1;
            break;
        }

        case IntegrationTechnique::LeftRect: {
            for (std::uint64_t i = 0; i < N; ++i) {
                double y = f(a + i * h) - comp;
                double t = sum + y;
                comp = (t - sum) - y;
                sum  = t;
            }

            ir.value = h * sum;
            ir.function_evaluations = N;
            break;
        }

        case IntegrationTechnique::RightRect: {
            for (std::uint64_t i = 1; i <= N; ++i) {
                double y = f(a + i * h) - comp;
                double t = sum + y;
                comp = (t - sum) - y;
                sum  = t;
            }

            ir.value = h * sum;
            ir.function_evaluations = N;
            break;
        }

        case IntegrationTechnique::MidpointRect: {
            for (std::uint64_t i = 0; i < N; ++i) {
                double y = f(a + (i + 0.5) * h) - comp;
                double t = sum + y;
                comp = (t - sum) - y;
                sum  = t;
            }

            ir.value = h * sum;
            ir.function_evaluations = N;
            break;
        }

        default: break;
    }

    ir.error_estimate = 0.0;
    ir.converged = true;
    return ir;
}

auto BasicIntegration::global_adaptive_simple(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    IntegrationTechnique rule
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = rule;
    std::priority_queue<SimpleRuleSubInterval> pq;
    pq.emplace(a, b, f, rule);
    ir.function_evaluations += 5;  
    double total     = 0.0;
    double total_comp = 0.0;
    double total_err  = 0.0;
    double err_comp   = 0.0;

    while (!pq.empty() && pq.size() < cfg.max_subdivisions) {
        SimpleRuleSubInterval top = pq.top();
        pq.pop();

        if (top.err < cfg.tolerance) {
            double y = top.S2 - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = top.err - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
            continue;
        }

        auto [L, R] = top.split(f);
        ir.function_evaluations += 10;   
        pq.push(L);
        pq.push(R);
    }

    while (!pq.empty()) {
        SimpleRuleSubInterval top = pq.top();
        pq.pop();
        double y = top.S2 - total_comp;
        double t = total + y;
        total_comp = (t - total) - y;
        total = t;
        double ye = top.err - err_comp;
        double te = total_err + ye;
        err_comp = (te - total_err) - ye;
        total_err = te;
    }

    ir.value          = total;
    ir.error_estimate = total_err;
    ir.converged      = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::hybrid_adaptive_simple(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    IntegrationTechnique rule
) -> IntegrationResult {
    std::priority_queue<SimpleRuleSubInterval> pq;
    pq.emplace(a, b, f, rule);
    IntegrationResult ir;
    ir.method = rule;
    ir.function_evaluations += 5;

    for (std::uint64_t i = 0; i < cfg.global_levels; ++i) {
        if (pq.empty()) break;
        SimpleRuleSubInterval top = pq.top();
        pq.pop();
        auto [L, R] = top.split(f);
        ir.function_evaluations += 10;
        pq.push(L);
        pq.push(R);
    }

    double total     = 0.0;
    double total_comp = 0.0;
    double total_err  = 0.0;
    double err_comp   = 0.0;

    while (!pq.empty()) {
        SimpleRuleSubInterval s = pq.top();
        pq.pop();

        if (s.err < cfg.tolerance) {
            double y = s.S2 - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = s.err - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
        } else {
            IntegrationConfig local_cfg = cfg;

            local_cfg.max_subdivisions = std::max<std::uint64_t>(
                64, cfg.max_subdivisions / std::max<std::uint64_t>(1, pq.size() + 1)
            );

            IntegrationResult local = run_local_simple(f, s.a, s.b, local_cfg, rule);
            ir.function_evaluations += local.function_evaluations;
            double y = local.value - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = s.err - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
        }
    }

    ir.value          = total;
    ir.error_estimate = total_err;
    ir.converged      = (total_err < cfg.tolerance);
    return ir;
}

std::vector<std::pair<double, double>> BasicIntegration::cc_nodes_weights(std::size_t N) {
    std::vector<std::pair<double, double>> out(N);
    const std::uint64_t n = N - 1;
    const std::uint64_t n_half = n / 2;
    std::vector<double> denom(n_half + 1);
    for (std::uint64_t j = 0; j <= n_half; ++j) { denom[j] = 1.0 / (1.0 - 4.0 * j * j); }

    for (std::uint64_t k = 0; k < N; ++k) {
        double node = std::cos(constants::pi() * k / n);
        double c_k  = (k == 0 || k == n) ? 1.0 : 2.0;
        double w    = 0.0;

        for (std::uint64_t j = 0; j <= n_half; ++j) {
            double b_j = (j == 0 || 2 * j == n) ? 1.0 : 2.0;
            w += b_j * std::cos(2.0 * constants::pi() * j * k / n) * denom[j];
        }

        out[k] = { node, c_k * w / n };
    }

    return out;
}

double BasicIntegration::cc_panel(
    const std::function<double(double)>& f,
    double a, double b,
    const std::pair<double, double>* nw, std::size_t count
) {
    const double mid      = 0.5 * (a + b);
    const double half_len = 0.5 * (b - a);
    double sum  = 0.0;
    double comp = 0.0;

    for (std::size_t i = 0; i < count; ++i) {
        double y = nw[i].second * f(mid + half_len * nw[i].first) - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum  = t;
    }

    return sum * half_len;
}

double BasicIntegration::cc_local_recursive(
    const std::function<double(double)>& f,
    double a, double b,
    double tolerance, double S,
    std::uint64_t depth, std::uint64_t max_depth,
    const std::pair<double, double>* nw, std::size_t count,
    std::uint64_t& total_evals
) {
    const double mid = 0.5 * (a + b);
    double I_left  = cc_panel(f, a, mid, nw, count);
    double I_right = cc_panel(f, mid, b, nw, count);
    total_evals += 2 * count;
    double I2  = I_left + I_right;
    double err = (I2 - S) / 15.0;
    double scale = std::max(std::abs(I2), std::abs(S));
    double tol = std::max(tolerance, scale * constants::type_epsilon() * 1000.0);
    if (depth >= max_depth || std::abs(err) <= tol) { return I2 + err;   }
    double h_l = mid - a;
    double h_r = b - mid;
    double span = b - a;

    return cc_local_recursive(f, a, mid, tolerance * h_l / span, I_left, depth + 1, max_depth, nw, count, total_evals)
         + cc_local_recursive(f, mid, b, tolerance * h_r / span, I_right, depth + 1, max_depth, nw, count, total_evals);
}

auto BasicIntegration::cc_global_adaptive(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = IntegrationTechnique::ClenshawCurtis;
    std::priority_queue<InternalSubInterval> pq;
    double S = cc_panel(f, a, b, nw, count);
    ir.function_evaluations += count;
    pq.push({ a, b, S, std::abs(S) * cfg.tolerance, 0 });
    double total      = 0.0;
    double total_comp = 0.0;
    double total_err  = 0.0;
    double err_comp   = 0.0;
    const double span = b - a;

    while (!pq.empty() && pq.size() < cfg.max_subdivisions) {
        InternalSubInterval top = pq.top();
        pq.pop();
        double mid    = 0.5 * (top.a + top.b);
        double I_left  = cc_panel(f, top.a, mid, nw, count);
        double I_right = cc_panel(f, mid, top.b, nw, count);
        ir.function_evaluations += 2 * count;

        double I2  = I_left + I_right;
        double err = std::abs((I2 - top.integral) / 15.0);

        if (err <= cfg.tolerance * (top.b - top.a) / span) {
            double val = I2 + (I2 - top.integral) / 15.0;
            double y = val - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = err - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
        } else {
            pq.push({ top.a, mid, I_left,  std::abs(I_left  - top.integral * 0.5), top.level + 1 });
            pq.push({ mid, top.b, I_right, std::abs(I_right - top.integral * 0.5), top.level + 1 });
        }
    }

    while (!pq.empty()) {
        InternalSubInterval top = pq.top();
        pq.pop();
        double y = top.integral - total_comp;
        double t = total + y;
        total_comp = (t - total) - y;
        total = t;
        double ye = top.error - err_comp;
        double te = total_err + ye;
        err_comp = (te - total_err) - ye;
        total_err = te;
    }

    ir.value          = total;
    ir.error_estimate = total_err;
    ir.converged      = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::cc_hybrid_adaptive(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = IntegrationTechnique::ClenshawCurtis;
    std::priority_queue<InternalSubInterval> pq;
    double S = cc_panel(f, a, b, nw, count);
    ir.function_evaluations += count;
    pq.push({ a, b, S, std::abs(S) * cfg.tolerance, 0 });
    const double span = b - a;

    for (std::uint64_t i = 0; i < cfg.global_levels; ++i) {
        if (pq.empty()) break;
        InternalSubInterval top = pq.top();
        pq.pop();
        double mid     = 0.5 * (top.a + top.b);
        double I_left  = cc_panel(f, top.a, mid, nw, count);
        double I_right = cc_panel(f, mid, top.b, nw, count);
        ir.function_evaluations += 2 * count;
        double I2  = I_left + I_right;
        double err = std::abs((I2 - top.integral) / 15.0);

        if (err <= cfg.tolerance * (top.b - top.a) / span) {
            pq.push({ top.a, top.b, I2 + (I2 - top.integral) / 15.0, 0.0, top.level + 1 });
        } else {
            pq.push({ top.a, mid, I_left,  std::abs(I_left  - top.integral * 0.5), top.level + 1 });
            pq.push({ mid, top.b, I_right, std::abs(I_right - top.integral * 0.5), top.level + 1 });
        }
    }

    double total      = 0.0;
    double total_comp = 0.0;
    double total_err  = 0.0;
    double err_comp   = 0.0;

    while (!pq.empty()) {
        InternalSubInterval s = pq.top();
        pq.pop();

        if (s.error <= constants::type_epsilon()) {
            double y = s.integral - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
        } else {
            double scaled_tol = std::max(
                cfg.tolerance * (s.b - s.a) / span,
                constants::type_epsilon() * std::abs(s.integral)
            );

            std::uint64_t evals = 0;

            double val = cc_local_recursive(
                f, s.a, s.b,
                scaled_tol, s.integral,
                0, cfg.max_depth,
                nw, count, evals
            );

            ir.function_evaluations += evals;
            double y = val - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = s.error - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
        }
    }

    ir.value          = total;
    ir.error_estimate = total_err;
    ir.converged      = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::cc_integrate_core(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult result;

    switch (cfg.strategy) {
        case AdaptiveStrategy::Local: {
            double S = cc_panel(f, a, b, nw, count);
            double tol = std::max(cfg.tolerance, constants::type_epsilon() * std::abs(S) * 10.0);
            std::uint64_t evals = count;
            double val = cc_local_recursive(f, a, b, tol, S, 0, cfg.max_depth, nw, count, evals);
            result.value = val;
            result.error_estimate = 0.0;
            result.converged = true;
            result.function_evaluations = evals;
            break;
        }

        case AdaptiveStrategy::Global:
            result = cc_global_adaptive(f, a, b, cfg, nw, count);
            break;

        case AdaptiveStrategy::Hybrid:
        default:
            result = cc_hybrid_adaptive(f, a, b, cfg, nw, count);
            break;
    }

    result.method = IntegrationTechnique::ClenshawCurtis;
    return result;
}

auto BasicIntegration::cc_integrate_preamble(
    std::function<double(double)> f, const Interval& iv,
    IntegrationConfig config,
    double coarse_epsilon, double fine_epsilon,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    if (iv.lower.value > iv.upper.value) {
        IntegrationResult switched = cc_integrate_preamble(
            f, Interval(iv.upper, iv.lower), config,
            coarse_epsilon, fine_epsilon, nw, count
        );

        switched.value = -switched.value;
        switched.method = IntegrationTechnique::ClenshawCurtis;
        return switched;
    }

    if (std::abs(iv.lower.value - iv.upper.value) <= constants::middle_epsilon()) {
        IntegrationResult ir;
        ir.converged = true;
        ir.method = IntegrationTechnique::ClenshawCurtis;
        return ir;
    }

    if (config.transform == InfiniteTransform::Auto) {
        config.transform = resolve_infinite_transform(iv, IntegrationTechnique::ClenshawCurtis);
    }

    clamp_value(coarse_epsilon, constants::type_epsilon(), 1.0);
    clamp_value(fine_epsilon, constants::type_epsilon(), 1.0);
    auto transformed_f = apply_infinite_transform(std::move(f), iv, config.transform);
    return cc_integrate_core(transformed_f.g, transformed_f.a, transformed_f.b, config, nw, count);
}

std::vector<std::pair<double, double>> BasicIntegration::gl_nodes_weights(std::size_t N) {
    N = std::max<std::size_t>(N, 1);
    std::vector<std::pair<double, double>> out(N);
    gl_compute_nodes_weights(out.data(), N);
    return out;
}

void BasicIntegration::gl_compute_nodes_weights(std::pair<double, double>* out, std::size_t N) {
    const double tol = constants::type_epsilon();
    const std::size_t m = (N + 1) / 2;

    for (std::size_t i = 0; i < m; ++i) {
        double z = std::cos(constants::pi() * (i + 0.75) / (N + 0.5));
        double pp = 0.0;

        for (;;) {
            double p1 = 1.0, p2 = 0.0;

            for (std::size_t j = 0; j < N; ++j) {
                double p3 = p2;
                p2 = p1;
                p1 = ((2.0 * j + 1.0) * z * p2 - j * p3) / (j + 1.0);
            }

            pp = static_cast<double>(N) * (z * p1 - p2) / (z * z - 1.0);
            double z_old = z;
            z -= p1 / pp;
            if (std::abs(z - z_old) <= tol) break;
        }

        double w = 2.0 / ((1.0 - z * z) * pp * pp);
        out[i]             = { -z, w };
        out[N - 1 - i]     = {  z, w };
    }
}

double BasicIntegration::gl_panel(
    const std::function<double(double)>& f,
    double a, double b,
    const std::pair<double, double>* nw, std::size_t count
) {
    const double mid      = 0.5 * (a + b);
    const double half_len = 0.5 * (b - a);
    double sum  = 0.0;
    double comp = 0.0;

    for (std::size_t i = 0; i < count; ++i) {
        double y = nw[i].second * f(mid + half_len * nw[i].first) - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum  = t;
    }

    return sum * half_len;
}

double BasicIntegration::gl_error_factor(std::size_t N) {
    constexpr double max_factor = 1.0 / constants::type_epsilon();

    double raw = (N <= 15)
        ? static_cast<double>((1ULL << (2 * N)) - 1)
        : std::pow(4.0, static_cast<double>(N)) - 1.0;

    return std::min(raw, max_factor);
}

double BasicIntegration::gl_local_recursive(
    const std::function<double(double)>& f,
    double a, double b,
    double tolerance, double S,
    std::uint64_t depth, std::uint64_t max_depth,
    const std::pair<double, double>* nw, std::size_t count,
    std::uint64_t& total_evals
) {
    const double mid = 0.5 * (a + b);
    double I_left  = gl_panel(f, a, mid, nw, count);
    double I_right = gl_panel(f, mid, b, nw, count);
    total_evals += 2 * count;
    double I2  = I_left + I_right;
    double ef  = gl_error_factor(count);
    double err = (I2 - S) / ef;
    double scale = std::max(std::abs(I2), std::abs(S));
    double tol   = std::max(tolerance, scale * constants::type_epsilon() * 1000.0);

    if (depth >= max_depth || std::abs(err) <= tol) {
        return I2 + err;
    }

    double h_l  = mid - a;
    double h_r  = b - mid;
    double span = b - a;

    return gl_local_recursive(f, a, mid, tolerance * h_l / span, I_left, depth + 1, max_depth, nw, count, total_evals)
         + gl_local_recursive(f, mid, b, tolerance * h_r / span, I_right, depth + 1, max_depth, nw, count, total_evals);
}

auto BasicIntegration::gl_global_adaptive(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = IntegrationTechnique::GaussLegendre;
    std::priority_queue<InternalSubInterval> pq;
    double S = gl_panel(f, a, b, nw, count);
    ir.function_evaluations += count;
    pq.push({ a, b, S, std::abs(S) * cfg.tolerance, 0 });
    double total      = 0.0;
    double total_comp = 0.0;
    double total_err  = 0.0;
    double err_comp   = 0.0;
    const double span = b - a;
    const double ef   = gl_error_factor(count);

    while (!pq.empty() && pq.size() < cfg.max_subdivisions) {
        InternalSubInterval top = pq.top();
        pq.pop();
        double mid     = 0.5 * (top.a + top.b);
        double I_left  = gl_panel(f, top.a, mid, nw, count);
        double I_right = gl_panel(f, mid, top.b, nw, count);
        ir.function_evaluations += 2 * count;
        double I2  = I_left + I_right;
        double err = std::abs((I2 - top.integral) / ef);

        if (err <= cfg.tolerance * (top.b - top.a) / span) {
            double val = I2 + (I2 - top.integral) / ef;
            double y = val - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;

            double ye = err - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
        } else {
            pq.push({ top.a, mid, I_left, std::abs(I_left - top.integral * 0.5), top.level + 1 });
            pq.push({ mid, top.b, I_right, std::abs(I_right - top.integral * 0.5), top.level + 1 });
        }
    }

    while (!pq.empty()) {
        InternalSubInterval top = pq.top();
        pq.pop();
        double y = top.integral - total_comp;
        double t = total + y;
        total_comp = (t - total) - y;
        total = t;
    }

    ir.value          = total;
    ir.error_estimate = total_err;
    ir.converged      = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::gl_hybrid_adaptive(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = IntegrationTechnique::GaussLegendre;
    std::priority_queue<InternalSubInterval> pq;
    double S = gl_panel(f, a, b, nw, count);
    ir.function_evaluations += count;
    pq.push({ a, b, S, std::abs(S) * cfg.tolerance, 0 });
    const double span = b - a;
    const double ef   = gl_error_factor(count);

    for (std::uint64_t i = 0; i < cfg.global_levels; ++i) {
        if (pq.empty()) break;
        InternalSubInterval top = pq.top();
        pq.pop();
        double mid     = 0.5 * (top.a + top.b);
        double I_left  = gl_panel(f, top.a, mid, nw, count);
        double I_right = gl_panel(f, mid, top.b, nw, count);
        ir.function_evaluations += 2 * count;
        double I2  = I_left + I_right;
        double err = std::abs((I2 - top.integral) / ef);

        if (err <= cfg.tolerance * (top.b - top.a) / span) {
            pq.push({ top.a, top.b, I2 + (I2 - top.integral) / ef, 0.0, top.level + 1 });
        } else {
            pq.push({ top.a, mid, I_left, std::abs(I_left - top.integral * 0.5), top.level + 1 });
            pq.push({ mid, top.b, I_right, std::abs(I_right - top.integral * 0.5), top.level + 1 });
        }
    }

    double total      = 0.0;
    double total_comp = 0.0;
    double total_err  = 0.0;
    double err_comp   = 0.0;

    while (!pq.empty()) {
        InternalSubInterval s = pq.top();
        pq.pop();

        if (s.error <= constants::type_epsilon()) {
            double y = s.integral - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
        } else {
            double scaled_tol = std::max(
                cfg.tolerance * (s.b - s.a) / span,
                constants::type_epsilon() * std::abs(s.integral)
            );

            std::uint64_t evals = 0;
            double val = gl_local_recursive(
                f, s.a, s.b,
                scaled_tol, s.integral,
                0, cfg.max_depth,
                nw, count, evals
            );

            ir.function_evaluations += evals;
            double y = val - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = s.error - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
        }
    }

    ir.value          = total;
    ir.error_estimate = total_err;
    ir.converged      = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::gl_integrate_core(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult result;

    switch (cfg.strategy) {
        case AdaptiveStrategy::Local: {
            double S = gl_panel(f, a, b, nw, count);
            double tol = std::max(cfg.tolerance, constants::type_epsilon() * std::abs(S) * 10.0);
            std::uint64_t evals = count;

            double val = gl_local_recursive(
                f, a, b, tol, S, 0, cfg.max_depth, nw, count, evals
            );

            result.value = val;
            result.error_estimate = 0.0;
            result.converged = true;
            result.function_evaluations = evals;
            break;
        }

        case AdaptiveStrategy::Global:
            result = gl_global_adaptive(f, a, b, cfg, nw, count);
            break;

        case AdaptiveStrategy::Hybrid:
        default:
            result = gl_hybrid_adaptive(f, a, b, cfg, nw, count);
            break;
    }

    result.method = IntegrationTechnique::GaussLegendre;
    return result;
}

auto BasicIntegration::gl_integrate_preamble(
    std::function<double(double)> f, const Interval& iv,
    IntegrationConfig config,
    double coarse_epsilon, double fine_epsilon,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    if (iv.lower.value > iv.upper.value) {
        IntegrationResult switched = gl_integrate_preamble(
            f, Interval(iv.upper, iv.lower), config,
            coarse_epsilon, fine_epsilon, nw, count
        );

        switched.value = -switched.value;
        switched.method = IntegrationTechnique::GaussLegendre;
        return switched;
    }

    if (std::abs(iv.lower.value - iv.upper.value) <= constants::middle_epsilon()) {
        IntegrationResult ir;
        ir.converged = true;
        ir.method = IntegrationTechnique::GaussLegendre;
        return ir;
    }

    if (config.transform == InfiniteTransform::Auto) {
        config.transform = resolve_infinite_transform(iv, IntegrationTechnique::GaussLegendre);
    }

    clamp_value(coarse_epsilon, constants::type_epsilon(), 1.0);
    clamp_value(fine_epsilon, constants::type_epsilon(), 1.0);
    auto transformed_f = apply_infinite_transform(std::move(f), iv, config.transform);

    return gl_integrate_core(
        transformed_f.g, transformed_f.a, transformed_f.b,
        config, nw, count
    );
}

std::vector<std::pair<double, double>> BasicIntegration::ts_nodes_weights(std::size_t N) {
    N = std::max<std::size_t>(N, 1);
    std::vector<std::pair<double, double>> out(N);
    ts_compute_nodes_weights(out.data(), N);
    return out;
}

void BasicIntegration::ts_compute_nodes_weights(std::pair<double, double>* out, std::size_t N) {
    constexpr double t_max = 3.15;
    const std::size_t m    = (N - 1) / 2;
    const double h         = (m > 0) ? t_max / static_cast<double>(m) : 1.0;
    std::size_t idx = 0;

    for (std::size_t i = m; i >= 1; --i) {
        double t        = static_cast<double>(i) * h;
        double sh       = std::sinh(t);
        double ch       = std::cosh(t);
        double arg      = constants::pi_2() * sh;
        double x        = std::tanh(arg);
        double cosh_arg = std::cosh(arg);
        double w        = h * constants::pi_2() * ch / (cosh_arg * cosh_arg);
        out[idx++] = { -x, w };
    }

    out[idx++] = { 0.0, h * constants::pi_2() };

    for (std::size_t i = 1; i <= m; ++i) {
        double t        = static_cast<double>(i) * h;
        double sh       = std::sinh(t);
        double ch       = std::cosh(t);
        double arg      = constants::pi_2() * sh;
        double x        = std::tanh(arg);
        double cosh_arg = std::cosh(arg);
        double w        = h * constants::pi_2() * ch / (cosh_arg * cosh_arg);
        out[idx++] = { x, w };
    }

    while (idx < N) { out[idx++] = { 0.0, 0.0 }; }
}

double BasicIntegration::ts_panel(
    const std::function<double(double)>& f,
    double a, double b,
    const std::pair<double, double>* nw, std::size_t count
) {
    const double mid      = 0.5 * (a + b);
    const double half_len = 0.5 * (b - a);
    double sum  = 0.0;
    double comp = 0.0;

    for (std::size_t i = 0; i < count; ++i) {
        if (nw[i].second == 0.0) continue;            
        double fval = f(mid + half_len * nw[i].first);
        if (!std::isfinite(fval)) continue;           
        double y = nw[i].second * fval - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum  = t;
    }

    return sum * half_len;
}

double BasicIntegration::ts_local_recursive(
    const std::function<double(double)>& f,
    double a, double b,
    double tolerance, double S,
    std::uint64_t depth, std::uint64_t max_depth,
    const std::pair<double, double>* nw, std::size_t count,
    std::uint64_t& total_evals
) {
    const double mid = 0.5 * (a + b);
    double I_left  = ts_panel(f, a, mid, nw, count);
    double I_right = ts_panel(f, mid, b, nw, count);
    total_evals += 2 * count;
    double I2  = I_left + I_right;
    double err = (I2 - S) / 15.0;                     

    double scale = std::max(std::abs(I2), std::abs(S));
    double tol   = std::max(tolerance, scale * constants::type_epsilon() * 1000.0);

    if (depth >= max_depth || std::abs(err) <= tol) {
        return I2 + err;
    }

    double h_l  = mid - a;
    double h_r  = b - mid;
    double span = b - a;

    return ts_local_recursive(f, a, mid, tolerance * h_l / span, I_left, depth + 1, max_depth, nw, count, total_evals)
         + ts_local_recursive(f, mid, b, tolerance * h_r / span, I_right, depth + 1, max_depth, nw, count, total_evals);
}

auto BasicIntegration::ts_global_adaptive(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = IntegrationTechnique::TanhSinh;
    std::priority_queue<InternalSubInterval> pq;
    double S = ts_panel(f, a, b, nw, count);
    ir.function_evaluations += count;
    pq.push({ a, b, S, std::abs(S) * cfg.tolerance, 0 });
    double total      = 0.0;
    double total_comp = 0.0;
    double total_err  = 0.0;
    double err_comp   = 0.0;
    const double span = b - a;

    while (!pq.empty() && pq.size() < cfg.max_subdivisions) {
        InternalSubInterval top = pq.top();
        pq.pop();
        double mid     = 0.5 * (top.a + top.b);
        double I_left  = ts_panel(f, top.a, mid, nw, count);
        double I_right = ts_panel(f, mid, top.b, nw, count);
        ir.function_evaluations += 2 * count;
        double I2  = I_left + I_right;
        double err = std::abs((I2 - top.integral) / 15.0);

        if (err <= cfg.tolerance * (top.b - top.a) / span) {
            double val = I2 + (I2 - top.integral) / 15.0;
            double y = val - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = err - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
        } else {
            pq.push({ top.a, mid, I_left, std::abs(I_left - top.integral * 0.5), top.level + 1 });
            pq.push({ mid, top.b, I_right, std::abs(I_right - top.integral * 0.5), top.level + 1 });
        }
    }

    while (!pq.empty()) {
        InternalSubInterval top = pq.top();
        pq.pop();
        double y = top.integral - total_comp;
        double t = total + y;
        total_comp = (t - total) - y;
        total = t;
    }

    ir.value          = total;
    ir.error_estimate = total_err;
    ir.converged      = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::ts_hybrid_adaptive(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult ir;
    ir.method = IntegrationTechnique::TanhSinh;
    std::priority_queue<InternalSubInterval> pq;
    double S = ts_panel(f, a, b, nw, count);
    ir.function_evaluations += count;
    pq.push({ a, b, S, std::abs(S) * cfg.tolerance, 0 });
    const double span = b - a;

    for (std::uint64_t i = 0; i < cfg.global_levels; ++i) {
        if (pq.empty()) break;
        InternalSubInterval top = pq.top();
        pq.pop();
        double mid     = 0.5 * (top.a + top.b);
        double I_left  = ts_panel(f, top.a, mid, nw, count);
        double I_right = ts_panel(f, mid, top.b, nw, count);
        ir.function_evaluations += 2 * count;
        double I2  = I_left + I_right;
        double err = std::abs((I2 - top.integral) / 15.0);

        if (err <= cfg.tolerance * (top.b - top.a) / span) {
            pq.push({ top.a, top.b,
                       I2 + (I2 - top.integral) / 15.0,
                       0.0, top.level + 1 });
        } else {
            pq.push({ top.a, mid, I_left, std::abs(I_left - top.integral * 0.5), top.level + 1 });
            pq.push({ mid, top.b, I_right, std::abs(I_right - top.integral * 0.5), top.level + 1 });
        }
    }

    double total      = 0.0;
    double total_comp = 0.0;
    double total_err  = 0.0;
    double err_comp   = 0.0;

    while (!pq.empty()) {
        InternalSubInterval s = pq.top();
        pq.pop();

        if (s.error <= constants::type_epsilon()) {
            double y = s.integral - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
        } else {
            double scaled_tol = std::max(
                cfg.tolerance * (s.b - s.a) / span,
                constants::type_epsilon() * std::abs(s.integral)
            );

            std::uint64_t evals = 0;

            double val = ts_local_recursive(
                f, s.a, s.b,
                scaled_tol, s.integral,
                0, cfg.max_depth,
                nw, count, evals
            );

            ir.function_evaluations += evals;
            double y = val - total_comp;
            double t = total + y;
            total_comp = (t - total) - y;
            total = t;
            double ye = s.error - err_comp;
            double te = total_err + ye;
            err_comp = (te - total_err) - ye;
            total_err = te;
        }
    }

    ir.value          = total;
    ir.error_estimate = total_err;
    ir.converged      = (total_err < cfg.tolerance);
    return ir;
}

auto BasicIntegration::ts_integrate_core(
    const std::function<double(double)>& f,
    double a, double b,
    const IntegrationConfig& cfg,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    IntegrationResult result;

    switch (cfg.strategy) {
        case AdaptiveStrategy::Local: {
            double S = ts_panel(f, a, b, nw, count);
            double tol = std::max(cfg.tolerance, constants::type_epsilon() * std::abs(S) * 10.0);
            std::uint64_t evals = count;

            double val = ts_local_recursive(
                f, a, b, tol, S, 0, cfg.max_depth, nw, count, evals
            );

            result.value = val;
            result.error_estimate = 0.0;
            result.converged = true;
            result.function_evaluations = evals;
            break;
        }

        case AdaptiveStrategy::Global:
            result = ts_global_adaptive(f, a, b, cfg, nw, count);
            break;

        case AdaptiveStrategy::Hybrid:
        default:
            result = ts_hybrid_adaptive(f, a, b, cfg, nw, count);
            break;
    }

    result.method = IntegrationTechnique::TanhSinh;
    return result;
}

auto BasicIntegration::ts_integrate_preamble(
    std::function<double(double)> f, const Interval& iv,
    IntegrationConfig config,
    double coarse_epsilon, double fine_epsilon,
    const std::pair<double, double>* nw, std::size_t count
) -> IntegrationResult {
    if (iv.lower.value > iv.upper.value) {
        IntegrationResult switched = ts_integrate_preamble(
            f, Interval(iv.upper, iv.lower), config,
            coarse_epsilon, fine_epsilon, nw, count
        );

        switched.value = -switched.value;
        switched.method = IntegrationTechnique::TanhSinh;
        return switched;
    }

    if (std::abs(iv.lower.value - iv.upper.value) <= constants::middle_epsilon()) {
        IntegrationResult ir;
        ir.converged = true;
        ir.method = IntegrationTechnique::TanhSinh;
        return ir;
    }

    if (config.transform == InfiniteTransform::Auto) {
        config.transform = resolve_infinite_transform(iv, IntegrationTechnique::TanhSinh);
    }

    clamp_value(coarse_epsilon, constants::type_epsilon(), 1.0);
    clamp_value(fine_epsilon, constants::type_epsilon(), 1.0);
    auto transformed_f = apply_infinite_transform(std::move(f), iv, config.transform);

    return ts_integrate_core(
        transformed_f.g, transformed_f.a, transformed_f.b,
        config, nw, count
    );
}

} // namespace integration
} // namespace math
} // namespace fizmo
