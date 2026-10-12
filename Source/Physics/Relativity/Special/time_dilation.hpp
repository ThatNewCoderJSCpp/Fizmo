#ifndef FIZMO_TIME_DILATION_HPP
#define FIZMO_TIME_DILATION_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

template <class N>
constexpr N dilated_time_(const N& beta, const N& t) {
    if (neg_(t)) return nan_<N>();
    return gamma_(beta) * t;
}

template <class N>
constexpr N proper_time_(const N& beta, const N& t) {
    if (neg_(t)) return nan_<N>();
    return s_(beta) * t;
}

template <class N>
constexpr N time_dilation_velocity_(const N& stationary, const N& object) {
    if (!(stationary > N(0)) || !(object > N(0))) return nan_<N>();
    const N r = object / stationary;
    if (r > N(1)) return nan_<N>();
    return sqrt_((N(1) - r) * (N(1) + r));
}

} // namespace rdetail

template <typename V, typename T, class RU = units::TimeUnit,
          rdetail::enable_t<rdetail::all_values<V, T>::value && rdetail::has_dim<RU>(units::DIMENSION_TIME)> = 0>
constexpr rdetail::result_t<V, T> dilated_time(const V& object_velocity, const T& object_observed_time, const RU return_unit = units::time::second) {
    return rdetail::run<rdetail::result_t<V, T>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::dilated_time_(rdetail::to_<N>(object_velocity), rdetail::to_<N>(object_observed_time)), units::time::second, return_unit);
    });
}

template <typename V, typename T, class VU, class TU, class RU = units::TimeUnit,
          rdetail::enable_t<rdetail::all_values<V, T>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<TU>(units::DIMENSION_TIME) && rdetail::has_dim<RU>(units::DIMENSION_TIME)> = 0>
constexpr rdetail::result_t<V, T> dilated_time(const V& object_velocity, const T& object_observed_time, const VU velocity_unit, const TU time_unit, const RU return_unit = units::time::second) {
    return rdetail::run<rdetail::result_t<V, T>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v  = rdetail::in_<N>(object_velocity, velocity_unit, units::velocity::light_speed);
        const N tt = rdetail::in_<N>(object_observed_time, time_unit, units::time::second);
        return rdetail::conv(rdetail::dilated_time_(v, tt), units::time::second, return_unit);
    });
}

template <typename V, typename T, class RU = units::TimeUnit,
          rdetail::enable_t<rdetail::all_values<V, T>::value && rdetail::has_dim<RU>(units::DIMENSION_TIME)> = 0>
constexpr rdetail::result_t<V, T> proper_time(const V& object_velocity, const T& stationary_observed_time, const RU return_unit = units::time::second) {
    return rdetail::run<rdetail::result_t<V, T>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::proper_time_(rdetail::to_<N>(object_velocity), rdetail::to_<N>(stationary_observed_time)), units::time::second, return_unit);
    });
}

template <typename V, typename T, class VU, class TU, class RU = units::TimeUnit,
          rdetail::enable_t<rdetail::all_values<V, T>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<TU>(units::DIMENSION_TIME) && rdetail::has_dim<RU>(units::DIMENSION_TIME)> = 0>
constexpr rdetail::result_t<V, T> proper_time(const V& object_velocity, const T& stationary_observed_time, const VU velocity_unit, const TU time_unit, const RU return_unit = units::time::second) {
    return rdetail::run<rdetail::result_t<V, T>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v  = rdetail::in_<N>(object_velocity, velocity_unit, units::velocity::light_speed);
        const N tt = rdetail::in_<N>(stationary_observed_time, time_unit, units::time::second);
        return rdetail::conv(rdetail::proper_time_(v, tt), units::time::second, return_unit);
    });
}

template <typename S, typename O, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<S, O>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<S, O> time_dilation_velocity(const S& stationary_observed_time, const O& object_observed_time, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<S, O>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::time_dilation_velocity_(rdetail::to_<N>(stationary_observed_time), rdetail::to_<N>(object_observed_time)), units::velocity::light_speed, return_unit);
    });
}

template <typename S, typename O, class STU, class OTU, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<S, O>::value && rdetail::has_dim<STU>(units::DIMENSION_TIME) && rdetail::has_dim<OTU>(units::DIMENSION_TIME) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<S, O> time_dilation_velocity(const S& stationary_observed_time, const O& object_observed_time, const STU stationary_time_unit, const OTU object_time_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<S, O>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N ts = rdetail::in_<N>(stationary_observed_time, stationary_time_unit, units::time::second);
        const N to = rdetail::in_<N>(object_observed_time, object_time_unit, units::time::second);
        return rdetail::conv(rdetail::time_dilation_velocity_(ts, to), units::velocity::light_speed, return_unit);
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_TIME_DILATION_HPP
