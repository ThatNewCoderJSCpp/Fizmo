#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace integration {

auto Integrator::integrate(
        std::function<double(double)> f,
        double a, double b,
        const IntegrationLayer& layer 
) -> IntegrationResult {
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

auto Integrator::make_function(const cas::Expression& e, const std::string& var) -> std::function<double(double)> {
        cas::Expression captured = e;
        std::string v = var;
        return [captured, v](double t) -> double { return captured.evaluate({{ v, t }}); };
    }

} // namespace integration
} // namespace math
} // namespace fizmo
