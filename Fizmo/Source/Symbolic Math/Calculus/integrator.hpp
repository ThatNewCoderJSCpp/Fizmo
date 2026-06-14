#ifndef FIZMO_MATH_INTEGRATOR_HPP
#define FIZMO_MATH_INTEGRATOR_HPP

#include "definite_integrator_1d.hpp"
#include "../CAS/expression_wrapper.hpp"
#include "../Common/main_convenience.hpp"
#include "../../Basic/constants.hpp"

#include <string>
#include <functional>
#include <stdexcept>
#include <utility>

namespace fizmo {
namespace math {
namespace integration {

struct IntegrationLayer {
    IntegrationTechnique technique = IntegrationTechnique::GaussKronrod;
    IntegrationConfig    config    = {};
    std::size_t          n         = 30;  

    double coarse_epsilon = constants::middle_epsilon();
    double fine_epsilon   = constants::type_epsilon();
};

class Integrator {
    Integrator() = delete;

public:
    static IntegrationResult integrate(
        std::function<double(double)> f,
        double a, double b,
        const IntegrationLayer& layer = {}
    ) {
        const auto& cfg = layer.config;
        const auto  ce  = layer.coarse_epsilon;
        const auto  fe  = layer.fine_epsilon;

        switch (layer.technique) {
            case IntegrationTechnique::Simpson:
                return BasicIntegration::integrate_simpson(std::move(f), a, b, cfg, ce, fe);
            case IntegrationTechnique::GaussLegendre:
                return BasicIntegration::integrate_gauss_legendre(std::move(f), a, b, cfg, ce, fe);
            case IntegrationTechnique::GaussKronrod:
                return BasicIntegration::integrate_gauss_kronrod(std::move(f), a, b, cfg, ce, fe);
            case IntegrationTechnique::Trapezoidial:
                return BasicIntegration::integrate_trapezoidal(std::move(f), a, b, cfg, ce, fe);
            case IntegrationTechnique::Romberg:
                return BasicIntegration::integrate_romberg(std::move(f), a, b, cfg, ce, fe);
            case IntegrationTechnique::LeftRect:
                return BasicIntegration::integrate_left_rect(std::move(f), a, b, cfg, ce, fe);
            case IntegrationTechnique::RightRect:
                return BasicIntegration::integrate_right_rect(std::move(f), a, b, cfg, ce, fe);
            case IntegrationTechnique::MidpointRect:
                return BasicIntegration::integrate_midpoint_rect(std::move(f), a, b, cfg, ce, fe);
            case IntegrationTechnique::ClenshawCurtis:
                return BasicIntegration::integrate_clenshaw_curtis(std::move(f), a, b, layer.n, cfg, ce, fe);
            case IntegrationTechnique::TanhSinh:
                return BasicIntegration::integrate_tanh_sinh(std::move(f), a, b, layer.n, cfg, ce, fe);

            case IntegrationTechnique::General:
            default:
                return BasicIntegration::integrate_gauss_kronrod(std::move(f), a, b, cfg, ce, fe);
        }
    }

    static IntegrationResult integrate(
        std::function<double(double)> f,
        const Interval& iv,
        const IntegrationLayer& layer = {}
    ) {
        return integrate(std::move(f), iv.lower.value, iv.upper.value, layer);
    }

    static IntegrationResult integrate(
        const cas::Expression& expr, const std::string& var,
        double a, double b,
        const IntegrationLayer& layer = {}
    ) {
        return integrate(make_function(expr, var), a, b, layer);
    }

    static IntegrationResult integrate(
        const cas::Expression& expr, const std::string& var,
        const Interval& iv,
        const IntegrationLayer& layer = {}
    ) {
        return integrate(make_function(expr, var), iv, layer);
    }

private:
    static std::function<double(double)> make_function(const cas::Expression& e, const std::string& var) {
        cas::Expression captured = e;
        std::string v = var;
        return [captured, v](double t) -> double { return captured.evaluate({{ v, t }}); };
    }
};

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATOR_HPP