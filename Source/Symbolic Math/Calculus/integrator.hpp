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
    );

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
    static std::function<double(double)> make_function(const cas::Expression& e, const std::string& var);
};

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATOR_HPP