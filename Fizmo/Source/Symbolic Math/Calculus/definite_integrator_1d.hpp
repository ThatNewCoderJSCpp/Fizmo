#ifndef DEFINITE_INTEGRATOR_FUNCTIONS_HPP
#define DEFINITE_INTEGRATOR_FUNCTIONS_HPP

#include "integral_interval.hpp"
#include "../../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace math {
namespace integration {

class BasicIntegration {
public:
    static double simpson_rule(double h, double fa, double fm, double fb) noexcept { return h / 6.0 * (fa + 4.0 * fm + fb); }
    static double simpson_rule(const std::function<double(double)>& f, double h, double a, double b) noexcept { return simpson_rule(h, f(a), f((a + b) * 0.5), f(b)); }

public:
    static double trapezoid_rule(double h, double fa, double fb) noexcept { return 0.5 * h * (fa + fb); }
    static double left_rect_rule(double h, double fa) noexcept { return h * fa; }
    static double right_rect_rule(double h, double fb) noexcept { return h * fb; }
    static double midpoint_rule(double h, double fm) noexcept { return h * fm; }

public:
    static IntegrationResult integrate_simpson(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_simpson(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    static IntegrationResult integrate_gauss_legendre(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_gauss_legendre(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    static IntegrationResult integrate_gauss_kronrod(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_gauss_kronrod(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    static IntegrationResult integrate_trapezoidal(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_trapezoidal(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    static IntegrationResult integrate_left_rect(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_left_rect(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    static IntegrationResult integrate_right_rect(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_right_rect(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    static IntegrationResult integrate_midpoint_rect(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_midpoint_rect(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    static IntegrationResult integrate_romberg(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_romberg(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    template <std::size_t N>
    static IntegrationResult integrate_clenshaw_curtis(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    ) {
        static_assert(N >= 3, "Clenshaw-Curtis requires at least 3 points");
        const auto& nw = cc_nodes_weights<N>();
        return cc_integrate_preamble(std::move(f), iv, config, coarse_epsilon, fine_epsilon, nw.data(), N);
    }

    template <std::size_t N>
    static IntegrationResult integrate_clenshaw_curtis(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    ) {
        return integrate_clenshaw_curtis<N>(std::move(f), Interval(Endpoint(a, true), Endpoint(b, true)), config, coarse_epsilon, fine_epsilon);
    }

    static IntegrationResult integrate_clenshaw_curtis(
        std::function<double(double)> f, const Interval& iv,
        std::size_t n,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_clenshaw_curtis(
        std::function<double(double)> f, double a, double b,
        std::size_t n,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    template <std::size_t N>
    static IntegrationResult integrate_gauss_legendre(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    ) {
        static_assert(N >= 1, "Gauss-Legendre requires at least 1 point");
        const auto& nw = gl_nodes_weights<N>();
        return gl_integrate_preamble(
            std::move(f), iv, config,
            coarse_epsilon, fine_epsilon, nw.data(), N
        );
    }

    template <std::size_t N>
    static IntegrationResult integrate_gauss_legendre(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    ) {
        return integrate_gauss_legendre<N>(
            std::move(f),
            Interval(Endpoint(a, true), Endpoint(b, true)),
            config, coarse_epsilon, fine_epsilon
        );
    }

    static IntegrationResult integrate_gauss_legendre(
        std::function<double(double)> f, const Interval& iv,
        std::size_t n,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_gauss_legendre(
        std::function<double(double)> f, double a, double b,
        std::size_t n,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

public:
    template <std::size_t N>
    static IntegrationResult integrate_tanh_sinh(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    ) {
        static_assert(N >= 1, "Tanh-Sinh requires at least 1 point");
        const auto& nw = ts_nodes_weights<N>();
        
        return ts_integrate_preamble(
            std::move(f), iv, config,
            coarse_epsilon, fine_epsilon, nw.data(), N
        );
    }

    template <std::size_t N>
    static IntegrationResult integrate_tanh_sinh(
        std::function<double(double)> f, double a, double b,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    ) {
        return integrate_tanh_sinh<N>(
            std::move(f),
            Interval(Endpoint(a, true), Endpoint(b, true)),
            config, coarse_epsilon, fine_epsilon
        );
    }

    static IntegrationResult integrate_tanh_sinh(
        std::function<double(double)> f, const Interval& iv,
        std::size_t n,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

    static IntegrationResult integrate_tanh_sinh(
        std::function<double(double)> f, double a, double b,
        std::size_t n,
        IntegrationConfig config = {},
        double coarse_epsilon = constants::middle_epsilon(),
        double fine_epsilon   = constants::type_epsilon()
    );

private:
    static constexpr double DE_TRUNC = 6.0;

    struct SubInterval {
        double a, b;
        double fa, fm, fb;
        double S;
        double S2;
        double err;

        SubInterval(double a_, double b_, const std::function<double(double)>& f);

        bool operator<(const SubInterval& other) const { return err < other.err; }

        std::pair<SubInterval, SubInterval> split(const std::function<double(double)>& f) const {
            double m = 0.5 * (a + b);
            return { SubInterval(a, m, f), SubInterval(m, b, f) };
        }
    };

    struct SimpleRuleSubInterval {
        double a, b;
        double fa, fm, fb;
        double S, S2, err;
        IntegrationTechnique rule;

        SimpleRuleSubInterval(double a_, double b_, const std::function<double(double)>& f, IntegrationTechnique rule_);

        bool operator<(const SimpleRuleSubInterval& o) const { return err < o.err; }

        std::pair<SimpleRuleSubInterval, SimpleRuleSubInterval> split(const std::function<double(double)>& f) const;
    };

    struct InternalSubInterval {
        double a, b;
        double integral;
        double error;
        std::uint64_t level;
        bool operator<(const InternalSubInterval& o) const { return error < o.error; }
    };

private:
    static InfiniteTransform resolve_infinite_transform(BoundType bound, IntegrationTechnique technique) noexcept;

    static InfiniteTransform resolve_infinite_transform(const Interval& iv, IntegrationTechnique technique) noexcept { return resolve_infinite_transform(interval_to_bound_type(iv), technique); }

    static BoundType interval_to_bound_type(const Interval& iv) noexcept;

    struct TransformedIntegrand {
        std::function<double(double)> g;
        double a, b;
        double operator()(double t) { return g(t); }
    };

    static TransformedIntegrand apply_infinite_transform(std::function<double(double)> f, const Interval& iv, InfiniteTransform transform);

    static TransformedIntegrand apply_right_infinite(std::function<double(double)> f, double a, InfiniteTransform transform);

    static TransformedIntegrand apply_fully_infinite(std::function<double(double)> f, InfiniteTransform transform);

private:
    static double local_adaptive_simpson(
        const std::function<double(double)>& f, double a, double b,
        double fa, double fm, double fb,
        double S, double tol, int depth,
        IntegrationResult& ir
    );

    static IntegrationResult run_local_simpson(const std::function<double(double)>& f, double a, double b, const IntegrationConfig& cfg);

    static IntegrationResult global_adaptive_simpson(const std::function<double(double)>& f, double a, double b, const IntegrationConfig& cfg);

    static IntegrationResult hybrid_adaptive_simpson(const std::function<double(double)>& f, double a, double b, const IntegrationConfig& cfg);

private:
    static constexpr unsigned int GL_P = 31;

    static constexpr double gl_nodes[GL_P] = {
        -0.9970874818194772, -0.9846859096651525, -0.9625039250929497,
        -0.9307569978966481, -0.8897600299482711, -0.8399203201462673,
        -0.7817331484166250, -0.7157767845868532, -0.6427067229242603,
        -0.5632491614071493, -0.4781937820449025, -0.3883859016082329,
        -0.2947180699817016, -0.1981211993355706, -0.0995553121523415,
         0.0000000000000000,  0.0995553121523415,  0.1981211993355706,
         0.2947180699817016,  0.3883859016082329,  0.4781937820449025,
         0.5632491614071493,  0.6427067229242603,  0.7157767845868532,
         0.7817331484166250,  0.8399203201462673,  0.8897600299482711,
         0.9307569978966481,  0.9625039250929497,  0.9846859096651525,
         0.9970874818194772
    };

    static constexpr double gl_weights[GL_P] = {
        0.0074708315792485, 0.0173186207903106, 0.0270090191849794,
        0.0364322739123855, 0.0454937075272010, 0.0541030824249169,
        0.0621747865610284, 0.0696285832354103, 0.0763903865987767,
        0.0823929917615893, 0.0875767406084780, 0.0918901138936415,
        0.0952902429123195, 0.0977433353863287, 0.0992250112266723,
        0.0997205447934264, 0.0992250112266723, 0.0977433353863287,
        0.0952902429123195, 0.0918901138936415, 0.0875767406084780,
        0.0823929917615893, 0.0763903865987767, 0.0696285832354103,
        0.0621747865610284, 0.0541030824249169, 0.0454937075272010,
        0.0364322739123855, 0.0270090191849794, 0.0173186207903106,
        0.0074708315792485
    };

    static IntegrationResult gauss_legendre_core(const std::function<double(double)>& f, double a, double b);

private:
    static constexpr unsigned int GK_KS = 21;   // Kronrod points
    static constexpr unsigned int GK_GS = 10;   // embedded Gauss points

    static constexpr double gk_kronrod_nodes[GK_KS] = {
        -0.9956571630258081, -0.9739065285171717, -0.9301574913557082,
        -0.8650633666889845, -0.7808177265864169, -0.6794095682990244,
        -0.5627571346686047, -0.4333953941292472, -0.2943928627014602,
        -0.1488743389816312,  0.0000000000000000,  0.1488743389816312,
         0.2943928627014602,  0.4333953941292472,  0.5627571346686047,
         0.6794095682990244,  0.7808177265864169,  0.8650633666889845,
         0.9301574913557082,  0.9739065285171717,  0.9956571630258081
    };

    static constexpr double gk_kronrod_weights[GK_KS] = {
        0.0116946388673718, 0.0325581623079647, 0.0547558965743519,
        0.0750396748109199, 0.0931254545836976, 0.1093871588022976,
        0.1234919762620659, 0.1347092173114733, 0.1427759385770601,
        0.1477391049013385, 0.1494455540029169, 0.1477391049013385,
        0.1427759385770601, 0.1347092173114733, 0.1234919762620659,
        0.1093871588022976, 0.0931254545836976, 0.0750396748109199,
        0.0547558965743519, 0.0325581623079647, 0.0116946388673718
    };

    static constexpr double gk_gauss_weights[GK_GS] = {
        0.0666713443086881, 0.1494513491505806, 0.2190863625159820,
        0.2692667193099963, 0.2955242247147529, 0.2955242247147529,
        0.2692667193099963, 0.2190863625159820, 0.1494513491505806,
        0.0666713443086881
    };

    static constexpr unsigned int gk_gauss_indices[GK_GS] = {
        1, 3, 5, 7, 9, 11, 13, 15, 17, 19
    };

    static IntegrationResult gauss_kronrod_panel(
        const std::function<double(double)>& f,
        double a, double b
    );

    static IntegrationResult adaptive_gauss_kronrod_recursive(
        const std::function<double(double)>& f,
        double a, double b,
        double tolerance,
        std::uint64_t depth, std::uint64_t max_depth,
        std::uint64_t& total_evals
    );

    static IntegrationResult adaptive_gauss_kronrod(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg
    );

private:
    static double simple_panel(IntegrationTechnique rule, double h, double fa, double fm, double fb) noexcept;

    static IntegrationResult integrate_simple_rule(
        std::function<double(double)> f, const Interval& iv,
        IntegrationTechnique rule,
        IntegrationConfig config,
        double coarse_epsilon, double fine_epsilon
    );

    static IntegrationResult run_local_simple(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        IntegrationTechnique rule
    );

    static IntegrationResult global_adaptive_simple(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        IntegrationTechnique rule
    );

    static IntegrationResult hybrid_adaptive_simple(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        IntegrationTechnique rule
    );

public:
    template <std::size_t N>
    static const std::array<std::pair<double, double>, N>& cc_nodes_weights() {
        static const auto nw = [] {
            std::array<std::pair<double, double>, N> out{};
            constexpr std::uint64_t n = N - 1;
            constexpr std::uint64_t n_half = n / 2;
            std::array<double, n_half + 1> denom{};
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
        }();

        return nw;
    }

    static std::vector<std::pair<double, double>> cc_nodes_weights(std::size_t N);

private:
    static double cc_panel(
        const std::function<double(double)>& f,
        double a, double b,
        const std::pair<double, double>* nw, std::size_t count
    );

    static double cc_local_recursive(
        const std::function<double(double)>& f,
        double a, double b,
        double tolerance, double S,
        std::uint64_t depth, std::uint64_t max_depth,
        const std::pair<double, double>* nw, std::size_t count,
        std::uint64_t& total_evals
    );

    static IntegrationResult cc_global_adaptive(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult cc_hybrid_adaptive(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult cc_integrate_core(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult cc_integrate_preamble(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config,
        double coarse_epsilon, double fine_epsilon,
        const std::pair<double, double>* nw, std::size_t count
    );

public:
    template <std::size_t N>
    static const std::array<std::pair<double, double>, N>& gl_nodes_weights() {
        static_assert(N >= 1, "Gauss-Legendre requires at least 1 point");
        static const auto nw = [] {
            std::array<std::pair<double, double>, N> out{};
            gl_compute_nodes_weights(out.data(), N);
            return out;
        }();
        return nw;
    }

    static std::vector<std::pair<double, double>> gl_nodes_weights(std::size_t N);

private:
    static void gl_compute_nodes_weights(std::pair<double, double>* out, std::size_t N);

    static double gl_panel(
        const std::function<double(double)>& f,
        double a, double b,
        const std::pair<double, double>* nw, std::size_t count
    );

    static double gl_error_factor(std::size_t N);

    static double gl_local_recursive(
        const std::function<double(double)>& f,
        double a, double b,
        double tolerance, double S,
        std::uint64_t depth, std::uint64_t max_depth,
        const std::pair<double, double>* nw, std::size_t count,
        std::uint64_t& total_evals
    );

    static IntegrationResult gl_global_adaptive(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult gl_hybrid_adaptive(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult gl_integrate_core(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult gl_integrate_preamble(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config,
        double coarse_epsilon, double fine_epsilon,
        const std::pair<double, double>* nw, std::size_t count
    );

public:
    template <std::size_t N>
    static const std::array<std::pair<double, double>, N>& ts_nodes_weights() {
        static_assert(N >= 1, "Tanh-Sinh requires at least 1 point");

        static const auto nw = [] {
            std::array<std::pair<double, double>, N> out{};
            ts_compute_nodes_weights(out.data(), N);
            return out;
        }();

        return nw;
    }
    
    static std::vector<std::pair<double, double>> ts_nodes_weights(std::size_t N);

private:
    static void ts_compute_nodes_weights(std::pair<double, double>* out, std::size_t N);

    static double ts_panel(
        const std::function<double(double)>& f,
        double a, double b,
        const std::pair<double, double>* nw, std::size_t count
    );

    static double ts_local_recursive(
        const std::function<double(double)>& f,
        double a, double b,
        double tolerance, double S,
        std::uint64_t depth, std::uint64_t max_depth,
        const std::pair<double, double>* nw, std::size_t count,
        std::uint64_t& total_evals
    );

    static IntegrationResult ts_global_adaptive(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult ts_hybrid_adaptive(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult ts_integrate_core(
        const std::function<double(double)>& f,
        double a, double b,
        const IntegrationConfig& cfg,
        const std::pair<double, double>* nw, std::size_t count
    );

    static IntegrationResult ts_integrate_preamble(
        std::function<double(double)> f, const Interval& iv,
        IntegrationConfig config,
        double coarse_epsilon, double fine_epsilon,
        const std::pair<double, double>* nw, std::size_t count
    );
};

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // DEFINITE_INTEGRATOR_FUNCTIONS_HPP