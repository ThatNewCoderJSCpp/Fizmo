#ifndef FIZMO_INTEGRATOR_INTERVAL_HPP
#define FIZMO_INTEGRATOR_INTERVAL_HPP

#include "../Common/intervals.hpp"
#include "../CAS/expression.hpp"
#include "../../Basic/constants.hpp"
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <limits>

namespace fizmo {
namespace math {
namespace integration {

enum class InfiniteTransform : std::uint8_t {
    Rational = 0,  // x = a + t/(1-t)              
    Exponential,   // x = a - ln(1-t)             
    TanhSinh,      // x = tanh(pi/2 * sinh(t))     
    Tangent,       // x = tan(pi * t / 2)
    LogRational,   // x = ln(t/(1-t))
    Auto           // select based on interval type
};

enum class AdaptiveStrategy : std::uint8_t {
    Global = 0,
    Local,
    Hybrid
};

struct IntegrationConfig {
    double           tolerance        = constants::middle_epsilon();
    std::uint64_t    max_depth        = 60;
    std::uint64_t    max_subdivisions = 10000;
    std::uint64_t    global_levels    = 5;       
    InfiniteTransform transform       = InfiniteTransform::Auto;
    AdaptiveStrategy  strategy        = AdaptiveStrategy::Hybrid;
};

enum class BoundType : std::uint8_t {
    Finite = 0,       // [a, b]        both finite
    LeftInfinite,     // (-inf, b]     left  open
    RightInfinite,    // [a, +inf)     right open
    FullyInfinite     // (-inf, +inf)  both  open
};

enum class IntegrationTechnique : std::uint8_t {
    Simpson = 0,
    GaussLegendre,
    GaussKronrod,
    Romberg,
    Trapezoidial,
    LeftRect,
    MidpointRect,
    RightRect,
    ClenshawCurtis,
    TanhSinh,
    General
};

struct IntegrationResult {
    double value          = 0.0;
    double error_estimate = 0.0;
    bool   converged      = false;
    std::uint64_t function_evaluations = 0;
    IntegrationTechnique method = IntegrationTechnique::Simpson;
    explicit operator double() const noexcept { return value; }
};

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // FIZMO_INTEGRATOR_INTERVAL_HPP