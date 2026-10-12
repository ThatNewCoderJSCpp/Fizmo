#ifndef FIZMO_RELATIVISTIC_DOPPLER_EFFECT_HPP
#define FIZMO_RELATIVISTIC_DOPPLER_EFFECT_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

template <class N>
constexpr N doppler_factor_(const N& beta) {
    const N one(1);
    if (!((one - beta) * (one + beta) > N(0))) return nan_<N>();
    return sqrt_((one + beta) / (one - beta));
}

template <class N>
constexpr N doppler_source_frequency_(const N& beta, const N& observed) {
    if (neg_(observed)) return nan_<N>();
    return observed / doppler_factor_(beta);
}

template <class N>
constexpr N doppler_observed_frequency_(const N& beta, const N& source) {
    if (neg_(source)) return nan_<N>();
    return source * doppler_factor_(beta);
}

template <class N>
constexpr N doppler_source_velocity_(const N& source, const N& observed) {
    if (!(source > N(0)) || neg_(observed)) return nan_<N>();
    const N one(1);
    if (observed > source) {
        const N q = source / observed;
        return (one - q) * (one + q) / (one + q * q);
    }
    const N r = observed / source;
    return -((one - r) * (one + r)) / (one + r * r);
}

} // namespace rdetail

template <typename S, typename O, class RU = units::FrequencyUnit,
          rdetail::enable_t<rdetail::all_values<S, O>::value && rdetail::has_dim<RU>(units::DIMENSION_FREQUENCY)> = 0>
constexpr rdetail::result_t<S, O> doppler_source_frequency(const S& source_velocity, const O& observed_frequency, const RU return_unit = units::frequency::hertz) {
    return rdetail::run<rdetail::result_t<S, O>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::doppler_source_frequency_(rdetail::to_<N>(source_velocity), rdetail::to_<N>(observed_frequency)), units::frequency::hertz, return_unit);
    });
}

template <typename S, typename O, class VU, class FU, class RU = units::FrequencyUnit,
          rdetail::enable_t<rdetail::all_values<S, O>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<FU>(units::DIMENSION_FREQUENCY) && rdetail::has_dim<RU>(units::DIMENSION_FREQUENCY)> = 0>
constexpr rdetail::result_t<S, O> doppler_source_frequency(const S& source_velocity, const O& observed_frequency, const VU velocity_unit, const FU frequency_unit, const RU return_unit = units::frequency::hertz) {
    return rdetail::run<rdetail::result_t<S, O>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(source_velocity, velocity_unit, units::velocity::light_speed);
        const N o = rdetail::in_<N>(observed_frequency, frequency_unit, units::frequency::hertz);
        return rdetail::conv(rdetail::doppler_source_frequency_(v, o), units::frequency::hertz, return_unit);
    });
}

template <typename V, typename F, class RU = units::FrequencyUnit,
          rdetail::enable_t<rdetail::all_values<V, F>::value && rdetail::has_dim<RU>(units::DIMENSION_FREQUENCY)> = 0>
constexpr rdetail::result_t<V, F> doppler_observed_frequency(const V& source_velocity, const F& source_frequency, const RU return_unit = units::frequency::hertz) {
    return rdetail::run<rdetail::result_t<V, F>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::doppler_observed_frequency_(rdetail::to_<N>(source_velocity), rdetail::to_<N>(source_frequency)), units::frequency::hertz, return_unit);
    });
}

template <typename V, typename F, class VU, class FU, class RU = units::FrequencyUnit,
          rdetail::enable_t<rdetail::all_values<V, F>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<FU>(units::DIMENSION_FREQUENCY) && rdetail::has_dim<RU>(units::DIMENSION_FREQUENCY)> = 0>
constexpr rdetail::result_t<V, F> doppler_observed_frequency(const V& source_velocity, const F& source_frequency, const VU velocity_unit, const FU frequency_unit, const RU return_unit = units::frequency::hertz) {
    return rdetail::run<rdetail::result_t<V, F>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(source_velocity, velocity_unit, units::velocity::light_speed);
        const N s = rdetail::in_<N>(source_frequency, frequency_unit, units::frequency::hertz);
        return rdetail::conv(rdetail::doppler_observed_frequency_(v, s), units::frequency::hertz, return_unit);
    });
}

template <typename S, typename O, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<S, O>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<S, O> doppler_source_velocity(const S& source_frequency, const O& observed_frequency, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<S, O>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::doppler_source_velocity_(rdetail::to_<N>(source_frequency), rdetail::to_<N>(observed_frequency)), units::velocity::light_speed, return_unit);
    });
}

template <typename S, typename O, class SU, class OU, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<S, O>::value && rdetail::has_dim<SU>(units::DIMENSION_FREQUENCY) && rdetail::has_dim<OU>(units::DIMENSION_FREQUENCY) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<S, O> doppler_source_velocity(const S& source_frequency, const O& observed_frequency, const SU source_unit, const OU observed_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<S, O>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N s = rdetail::in_<N>(source_frequency, source_unit, units::frequency::hertz);
        const N o = rdetail::in_<N>(observed_frequency, observed_unit, units::frequency::hertz);
        return rdetail::conv(rdetail::doppler_source_velocity_(s, o), units::velocity::light_speed, return_unit);
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_DOPPLER_EFFECT_HPP
