#ifndef FIZMO_RELATIVISTIC_VELOCITY_ADDITION_HPP
#define FIZMO_RELATIVISTIC_VELOCITY_ADDITION_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

template <class N>
constexpr N add_velocities_(const N& a, const N& b) {
    const N denom = N(1) + a * b;
    if (denom == N(0)) return nan_<N>();
    return (a + b) / denom;
}

template <class N>
constexpr N subtract_velocities_(const N& total, const N& part) {
    const N denom = N(1) - total * part;
    if (denom == N(0)) return nan_<N>();
    return (total - part) / denom;
}

} // namespace rdetail

template <typename V2, typename V3, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<V2, V3>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<V2, V3> velocity_addition_vel1(const V2& vel2, const V3& vel3, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<V2, V3>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::subtract_velocities_(rdetail::to_<N>(vel3), rdetail::to_<N>(vel2)), units::velocity::light_speed, return_unit);
    });
}

template <typename V2, typename V3, class U2, class U3, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<V2, V3>::value && rdetail::has_dim<U2>(units::DIMENSION_VELOCITY) && rdetail::has_dim<U3>(units::DIMENSION_VELOCITY) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<V2, V3> velocity_addition_vel1(const V2& vel2, const V3& vel3, const U2 vel2_unit, const U3 vel3_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<V2, V3>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v2 = rdetail::in_<N>(vel2, vel2_unit, units::velocity::light_speed);
        const N v3 = rdetail::in_<N>(vel3, vel3_unit, units::velocity::light_speed);
        return rdetail::conv(rdetail::subtract_velocities_(v3, v2), units::velocity::light_speed, return_unit);
    });
}

template <typename V1, typename V3, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<V1, V3>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<V1, V3> velocity_addition_vel2(const V1& vel1, const V3& vel3, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<V1, V3>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::subtract_velocities_(rdetail::to_<N>(vel3), rdetail::to_<N>(vel1)), units::velocity::light_speed, return_unit);
    });
}

template <typename V1, typename V3, class U1, class U3, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<V1, V3>::value && rdetail::has_dim<U1>(units::DIMENSION_VELOCITY) && rdetail::has_dim<U3>(units::DIMENSION_VELOCITY) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<V1, V3> velocity_addition_vel2(const V1& vel1, const V3& vel3, const U1 vel1_unit, const U3 vel3_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<V1, V3>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v1 = rdetail::in_<N>(vel1, vel1_unit, units::velocity::light_speed);
        const N v3 = rdetail::in_<N>(vel3, vel3_unit, units::velocity::light_speed);
        return rdetail::conv(rdetail::subtract_velocities_(v3, v1), units::velocity::light_speed, return_unit);
    });
}

template <typename V1, typename V2, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<V1, V2>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<V1, V2> velocity_addition_vel3(const V1& vel1, const V2& vel2, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<V1, V2>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::add_velocities_(rdetail::to_<N>(vel1), rdetail::to_<N>(vel2)), units::velocity::light_speed, return_unit);
    });
}

template <typename V1, typename V2, class U1, class U2, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<V1, V2>::value && rdetail::has_dim<U1>(units::DIMENSION_VELOCITY) && rdetail::has_dim<U2>(units::DIMENSION_VELOCITY) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<V1, V2> velocity_addition_vel3(const V1& vel1, const V2& vel2, const U1 vel1_unit, const U2 vel2_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<V1, V2>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v1 = rdetail::in_<N>(vel1, vel1_unit, units::velocity::light_speed);
        const N v2 = rdetail::in_<N>(vel2, vel2_unit, units::velocity::light_speed);
        return rdetail::conv(rdetail::add_velocities_(v1, v2), units::velocity::light_speed, return_unit);
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_VELOCITY_ADDITION_HPP
