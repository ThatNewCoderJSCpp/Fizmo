#ifndef FIZMO_INTEGRATOR_INTERVAL_HPP
#define FIZMO_INTEGRATOR_INTERVAL_HPP

#include "intervals.hpp"
#include "../Basic/constants.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizmo {
namespace math {
namespace integration {

enum class InfiniteTransform : std::uint8_t {
    Rational = 0,  // x = a + t/(1-t)
    Exponential,   // x = a - ln(1-t)
    TanhSinh,      // x = tanh(pi/2 * sinh(t))
    Tangent,       // x = tan(pi * t / 2)
    LogRational,   // x = ln(t/(1-t))
    Auto           
};

enum class AdaptiveStrategy : std::uint8_t {
    Global = 0,
    Local,
    Hybrid
};

enum class BoundType : std::uint8_t {
    Finite = 0,       
    LeftInfinite,     
    RightInfinite,    
    FullyInfinite     
};

enum class IntegrationTechnique : std::uint8_t {
    Simpson = 0,
    GaussLegendre,
    GaussKronrod,
    Romberg,
    LeftRect,
    MidpointRect,
    RightRect,
    ClenshawCurtis,
    TanhSinh,
    General,
    Trapezoidal 
};

inline const char* to_string(InfiniteTransform t) noexcept {
    switch (t) {
        case InfiniteTransform::Rational:    return "Rational";
        case InfiniteTransform::Exponential: return "Exponential";
        case InfiniteTransform::TanhSinh:    return "TanhSinh";
        case InfiniteTransform::Tangent:     return "Tangent";
        case InfiniteTransform::LogRational: return "LogRational";
        default:                             return "Auto";
    }
}

inline const char* to_string(AdaptiveStrategy s) noexcept {
    switch (s) {
        case AdaptiveStrategy::Global: return "Global";
        case AdaptiveStrategy::Local:  return "Local";
        default:                       return "Hybrid";
    }
}

inline const char* to_string(BoundType b) noexcept {
    switch (b) {
        case BoundType::Finite:        return "Finite";
        case BoundType::LeftInfinite:  return "LeftInfinite";
        case BoundType::RightInfinite: return "RightInfinite";
        default:                       return "FullyInfinite";
    }
}

inline const char* to_string(IntegrationTechnique m) noexcept {
    switch (m) {
        case IntegrationTechnique::Simpson:        return "Simpson";
        case IntegrationTechnique::GaussLegendre:  return "GaussLegendre";
        case IntegrationTechnique::GaussKronrod:   return "GaussKronrod";
        case IntegrationTechnique::Romberg:        return "Romberg";
        case IntegrationTechnique::Trapezoidal:    return "Trapezoidal";
        case IntegrationTechnique::LeftRect:       return "LeftRect";
        case IntegrationTechnique::MidpointRect:   return "MidpointRect";
        case IntegrationTechnique::RightRect:      return "RightRect";
        case IntegrationTechnique::ClenshawCurtis: return "ClenshawCurtis";
        case IntegrationTechnique::TanhSinh:       return "TanhSinh";
        default:                                   return "General";
    }
}

namespace intdetail {

template <typename T>
struct int_valid_value : std::integral_constant<bool, (std::is_floating_point<T>::value || fizmo::is_fizmo_float<T>::value) && std::is_same<T, typename std::remove_cv<T>::type>::value> {};

template <typename T>
constexpr bool int_valid_value_v = int_valid_value<T>::value;

template <typename T> auto int_abs(const T& x, ivdetail::iv_rank<2>) -> decltype(static_cast<T>(x.abs())) { return x.abs(); }
template <typename T> auto int_abs(const T& x, ivdetail::iv_rank<1>) -> typename std::enable_if<std::is_floating_point<T>::value, T>::type { return std::abs(x); }
template <typename T> T    int_abs(const T& x, ivdetail::iv_rank<0>) { return (x < T()) ? T(-x) : x; }

template <typename T> inline T abs(const T& x)                    { return int_abs(x, ivdetail::iv_rank<2>()); }
template <typename T> inline const T& max(const T& a, const T& b) { return (a < b) ? b : a; }

} // namespace intdetail

struct NoIntegrationContext {
    friend constexpr bool operator==(NoIntegrationContext, NoIntegrationContext) noexcept { return true; }
    friend constexpr bool operator!=(NoIntegrationContext, NoIntegrationContext) noexcept { return false; }
};

template <typename T, typename = void>
struct integration_traits;

template <typename T>
struct integration_traits<T, typename std::enable_if<std::is_floating_point<T>::value>::type> {
    using context_type = NoIntegrationContext;

    static context_type default_context() noexcept { return context_type(); }

    static T default_tolerance(const context_type&) noexcept {
        return std::is_same<T, double>::value ? static_cast<T>(fizmo::constants::middle_epsilon()) : static_cast<T>(std::sqrt(std::numeric_limits<T>::epsilon()));
    }

    static T default_relative_tolerance(const context_type&) noexcept { return T(0); }
};

template <typename T>
struct integration_traits<T, typename std::enable_if<fizmo::is_fizmo_float<T>::value && !std::is_floating_point<T>::value && !std::is_same<T, multiprecision::BigFloat>::value>::type> {
    static_assert(std::numeric_limits<T>::is_specialized, "integration_traits<T>: specialize std::numeric_limits<T> or integration_traits<T> for this type");

    using context_type = NoIntegrationContext;

    static context_type default_context() noexcept { return context_type(); }
    static T default_tolerance(const context_type&)          { return std::numeric_limits<T>::epsilon() * T(256u); }
    static T default_relative_tolerance(const context_type&) { return std::numeric_limits<T>::epsilon() * T(256u); }
};

template <>
struct integration_traits<multiprecision::BigFloat, void> {
    using context_type = multiprecision::BigFloatContext;

    static context_type default_context() { return multiprecision::BigFloatContext::current(); }

    static multiprecision::BigFloat default_tolerance(const context_type& ctx) {
        return multiprecision::BigFloat::one().scaled_pow2(8 - static_cast<std::int64_t>(ctx.precision));
    }

    static multiprecision::BigFloat default_relative_tolerance(const context_type& ctx) { return default_tolerance(ctx); }
};

template <typename T, typename = typename std::enable_if<intdetail::int_valid_value_v<T>>::type>
struct IntegrationConfig {
    using value_type   = T;
    using traits_type  = integration_traits<T>;
    using context_type = typename traits_type::context_type;

    T                 tolerance          = traits_type::default_tolerance(traits_type::default_context());
    std::uint64_t     max_depth          = 60;
    std::uint64_t     max_subdivisions   = 10000;
    std::uint64_t     global_levels      = 5;
    InfiniteTransform transform          = InfiniteTransform::Auto;
    AdaptiveStrategy  strategy           = AdaptiveStrategy::Hybrid;

    T                 relative_tolerance = traits_type::default_relative_tolerance(traits_type::default_context());
    context_type      context            = traits_type::default_context();

    static IntegrationConfig for_context(const context_type& ctx) {
        IntegrationConfig c;
        c.context            = ctx;
        c.tolerance          = traits_type::default_tolerance(ctx);
        c.relative_tolerance = traits_type::default_relative_tolerance(ctx);
        return c;
    }

    T tolerance_for(const T& value) const {
        const T rel = relative_tolerance * intdetail::abs(value);
        return intdetail::max(tolerance, rel);
    }

    bool accepts(const T& error, const T& value) const { return !(tolerance_for(value) < intdetail::abs(error)); }
};

template <typename T, typename = typename std::enable_if<intdetail::int_valid_value_v<T>>::type>
struct IntegrationResult {
    using value_type   = T;
    using context_type = typename integration_traits<T>::context_type;

    T                    value                = T();
    T                    error_estimate       = T();
    bool                 converged            = false;
    std::uint64_t        function_evaluations = 0;
    IntegrationTechnique method               = IntegrationTechnique::Simpson;

    std::uint64_t        subdivisions         = 0;
    context_type         context              = integration_traits<T>::default_context();   

    explicit operator T() const { return value; }

    IntegrationResult& accumulate(const IntegrationResult& o) {
        value                 = value + o.value;
        error_estimate        = error_estimate + o.error_estimate;
        converged             = converged && o.converged;
        function_evaluations += o.function_evaluations;
        subdivisions         += o.subdivisions;
        return *this;
    }

    static IntegrationResult converged_zero(IntegrationTechnique m) {               
        IntegrationResult r;
        r.converged = true;
        r.method    = m;
        return r;
    }
};

template <typename T>
inline BoundType bound_type_of(const Interval<T>& iv) noexcept {
    const bool li = iv.lower().is_infinite();
    const bool ui = iv.upper().is_infinite();
    if (li && ui) return BoundType::FullyInfinite;
    if (li)       return BoundType::LeftInfinite;
    if (ui)       return BoundType::RightInfinite;
    return BoundType::Finite;
}

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // FIZMO_INTEGRATOR_INTERVAL_HPP