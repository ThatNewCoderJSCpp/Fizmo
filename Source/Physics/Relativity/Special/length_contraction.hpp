#ifndef FIZMO_LENGTH_CONTRACTION_HPP
#define FIZMO_LENGTH_CONTRACTION_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

template <class N>
constexpr N contracted_length_(const N& beta, const N& l) {
    if (neg_(l)) return nan_<N>();
    return s_(beta) * l;
}

template <class N>
constexpr N original_length_(const N& beta, const N& l) {
    if (neg_(l)) return nan_<N>();
    return gamma_(beta) * l;
}

template <class N>
constexpr N length_contraction_velocity_(const N& original, const N& contracted) {
    if (!(original > N(0)) || neg_(contracted)) return nan_<N>();
    const N r = contracted / original;
    if (r > N(1)) return nan_<N>();
    return sqrt_((N(1) - r) * (N(1) + r));
}

} // namespace rdetail

template <typename L, typename V, class RU = units::LengthUnit,
          rdetail::enable_t<rdetail::all_values<L, V>::value && rdetail::has_dim<RU>(units::DIMENSION_LENGTH)> = 0>
constexpr rdetail::result_t<L, V> contracted_length(const V& object_velocity, const L& original_length, const RU return_unit = units::distance::meter) {
    return rdetail::run<rdetail::result_t<L, V>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::contracted_length_(rdetail::to_<N>(object_velocity), rdetail::to_<N>(original_length)), units::distance::meter, return_unit);
    });
}

template <typename L, typename V, class VU, class LU, class RU = units::LengthUnit,
          rdetail::enable_t<rdetail::all_values<L, V>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<LU>(units::DIMENSION_LENGTH) && rdetail::has_dim<RU>(units::DIMENSION_LENGTH)> = 0>
constexpr rdetail::result_t<L, V> contracted_length(const V& object_velocity, const L& original_length, const VU velocity_unit, const LU length_unit, const RU return_unit = units::distance::meter) {
    return rdetail::run<rdetail::result_t<L, V>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(object_velocity, velocity_unit, units::velocity::light_speed);
        const N l = rdetail::in_<N>(original_length, length_unit, units::distance::meter);
        return rdetail::conv(rdetail::contracted_length_(v, l), units::distance::meter, return_unit);
    });
}

template <typename L, typename V, class RU = units::LengthUnit,
          rdetail::enable_t<rdetail::all_values<L, V>::value && rdetail::has_dim<RU>(units::DIMENSION_LENGTH)> = 0>
constexpr rdetail::result_t<L, V> original_length(const V& object_velocity, const L& contracted_length, const RU return_unit = units::distance::meter) {
    return rdetail::run<rdetail::result_t<L, V>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::original_length_(rdetail::to_<N>(object_velocity), rdetail::to_<N>(contracted_length)), units::distance::meter, return_unit);
    });
}

template <typename L, typename V, class VU, class LU, class RU = units::LengthUnit,
          rdetail::enable_t<rdetail::all_values<L, V>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<LU>(units::DIMENSION_LENGTH) && rdetail::has_dim<RU>(units::DIMENSION_LENGTH)> = 0>
constexpr rdetail::result_t<L, V> original_length(const V& object_velocity, const L& contracted_length, const VU velocity_unit, const LU length_unit, const RU return_unit = units::distance::meter) {
    return rdetail::run<rdetail::result_t<L, V>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(object_velocity, velocity_unit, units::velocity::light_speed);
        const N l = rdetail::in_<N>(contracted_length, length_unit, units::distance::meter);
        return rdetail::conv(rdetail::original_length_(v, l), units::distance::meter, return_unit);
    });
}

template <typename O, typename C, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<O, C>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<C, O> length_contraction_velocity(const O& original_length, const C& contracted_length, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<C, O>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::length_contraction_velocity_(rdetail::to_<N>(original_length), rdetail::to_<N>(contracted_length)), units::velocity::light_speed, return_unit);
    });
}

template <typename O, typename C, class OU, class CU, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<O, C>::value && rdetail::has_dim<OU>(units::DIMENSION_LENGTH) && rdetail::has_dim<CU>(units::DIMENSION_LENGTH) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<C, O> length_contraction_velocity(const O& original_length, const C& contracted_length, const OU original_unit, const CU contracted_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<C, O>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N l  = rdetail::in_<N>(original_length, original_unit, units::distance::meter);
        const N lp = rdetail::in_<N>(contracted_length, contracted_unit, units::distance::meter);
        return rdetail::conv(rdetail::length_contraction_velocity_(l, lp), units::velocity::light_speed, return_unit);
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_LENGTH_CONTRACTION_HPP
