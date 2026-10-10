#ifndef FIZMO_MULTIPRECISION_CONSTANTS_HPP
#define FIZMO_MULTIPRECISION_CONSTANTS_HPP

#include "Details/basic_const_computing.hpp"

namespace fizmo {
namespace multiprecision {
namespace constants {

template <class F>
detail::require_float_t<F>
calculate_sqrt2() noexcept {
    return detail::materialize<F>::from(
        detail::sqrt2_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
sqrt2() noexcept {
    static const F value = calculate_sqrt2<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_sqrt2() noexcept {
    return detail::materialize<F>::from(
        detail::inv_sqrt2_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_sqrt2() noexcept {
    static const F value = calculate_inv_sqrt2<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_sqrt3() noexcept {
    return detail::materialize<F>::from(
        detail::sqrt3_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
sqrt3() noexcept {
    static const F value = calculate_sqrt3<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_sqrt3() noexcept {
    return detail::materialize<F>::from(
        detail::inv_sqrt3_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_sqrt3() noexcept {
    static const F value = calculate_inv_sqrt3<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_sqrt5() noexcept {
    return detail::materialize<F>::from(
        detail::sqrt5_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
sqrt5() noexcept {
    static const F value = calculate_sqrt5<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_sqrt5() noexcept {
    return detail::materialize<F>::from(
        detail::inv_sqrt5_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_sqrt5() noexcept {
    static const F value = calculate_inv_sqrt5<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_ln2() noexcept {
    return detail::materialize<F>::from(
        detail::ln2_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
ln2() noexcept {
    static const F value = calculate_ln2<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_ln2() noexcept {
    return detail::materialize<F>::from(
        detail::ln2_guard<detail::guard_of<F>>().reciprocal()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_ln2() noexcept {
    static const F value = calculate_inv_ln2<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_ln10() noexcept {
    return detail::materialize<F>::from(
        detail::ln10_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
ln10() noexcept {
    static const F value = calculate_ln10<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_ln10() noexcept {
    return detail::materialize<F>::from(
        detail::ln10_guard<detail::guard_of<F>>().reciprocal()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_ln10() noexcept {
    static const F value = calculate_inv_ln10<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_e() noexcept {
    return detail::materialize<F>::from(
        detail::e_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
e() noexcept {
    static const F value = calculate_e<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_e() noexcept {
    return detail::materialize<F>::from(
        detail::e_guard<detail::guard_of<F>>().reciprocal()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_e() noexcept {
    static const F value = calculate_inv_e<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_phi() noexcept {
    return detail::materialize<F>::from(
        detail::phi_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
phi() noexcept {
    static const F value = calculate_phi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_phi() noexcept {
    return detail::materialize<F>::from(
        detail::inv_phi_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_phi() noexcept {
    static const F value = calculate_inv_phi<F>();
    return value;
}

template <class F>
detail::require_signed_float_t<F>
calculate_psi() noexcept {
    return detail::negate_value<F>::from(
        calculate_inv_phi<F>()
    );
}

template <class F>
detail::require_signed_float_t<F, const F&>
psi() noexcept {
    static const F value = calculate_psi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_pi() noexcept {
    return detail::materialize<F>::from(
        detail::pi_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
pi() noexcept {
    static const F value = calculate_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_two_pi() noexcept {
    return detail::materialize<F>::from(
        fmath::scalb_i(
            detail::pi_guard<detail::guard_of<F>>(), 1
        )
    );
}

template <class F>
detail::require_float_t<F, const F&>
two_pi() noexcept {
    static const F value = calculate_two_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_half_pi() noexcept {
    return detail::materialize<F>::from(
        fmath::scalb_i(
            detail::pi_guard<detail::guard_of<F>>(), -1
        )
    );
}

template <class F>
detail::require_float_t<F, const F&>
half_pi() noexcept {
    static const F value = calculate_half_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_third_pi() noexcept {
    return detail::materialize<F>::from(
        fmath::div_u32(
            detail::pi_guard<detail::guard_of<F>>(), 3u
        )
    );
}

template <class F>
detail::require_float_t<F, const F&>
third_pi() noexcept {
    static const F value = calculate_third_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_quarter_pi() noexcept {
    return detail::materialize<F>::from(
        fmath::scalb_i(
            detail::pi_guard<detail::guard_of<F>>(), -2
        )
    );
}

template <class F>
detail::require_float_t<F, const F&>
quarter_pi() noexcept {
    static const F value = calculate_quarter_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_sixth_pi() noexcept {
    return detail::materialize<F>::from(
        fmath::div_u32(
            detail::pi_guard<detail::guard_of<F>>(), 6u
        )
    );
}

template <class F>
detail::require_float_t<F, const F&>
sixth_pi() noexcept {
    static const F value = calculate_sixth_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_pi_180() noexcept {
    return detail::materialize<F>::from(
        fmath::div_u32(
            detail::pi_guard<detail::guard_of<F>>(), 180u
        )
    );
}

template <class F>
detail::require_float_t<F, const F&>
pi_180() noexcept {
    static const F value = calculate_pi_180<F>();
    return value;
}


template <class F>
detail::require_float_t<F>
calculate_inv_pi() noexcept {
    return detail::materialize<F>::from(
        detail::pi_guard<detail::guard_of<F>>().reciprocal()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_pi() noexcept {
    static const F value = calculate_inv_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_two_inv_pi() noexcept {
    return detail::materialize<F>::from(
        fmath::scalb_i(
            detail::pi_guard<detail::guard_of<F>>().reciprocal(), 1
        )
    );
}

template <class F>
detail::require_float_t<F, const F&>
two_inv_pi() noexcept {
    static const F value = calculate_two_inv_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_two_pi() noexcept {
    return detail::materialize<F>::from(
        fmath::scalb_i(
            detail::pi_guard<detail::guard_of<F>>().reciprocal(), -1
        )
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_two_pi() noexcept {
    static const F value = calculate_inv_two_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_pi_180() noexcept {
    return detail::materialize<F>::from(
        detail::guard_of<F>(180)
        * detail::pi_guard<detail::guard_of<F>>().reciprocal()
    );
}

template <class F>
detail::require_float_t<F, const F&>
inv_pi_180() noexcept {
    static const F value = calculate_inv_pi_180<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_zeta2() noexcept {
    using G = detail::guard_of<F>;
    const G& p = detail::pi_guard_c<G>();
    return detail::materialize<F>::from(fmath::div_u32(p * p, 6u));
}

template <class F>
detail::require_float_t<F, const F&>
zeta2() noexcept {
    static const F value = calculate_zeta2<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_apery() noexcept {
    return detail::materialize<F>::from(
        detail::zeta3_guard<detail::guard_of<F>>()
    );
}

template <class F>
detail::require_float_t<F, const F&>
apery() noexcept {
    static const F value = calculate_apery<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_sqrt_pi() noexcept {
    using G = detail::guard_of<F>;
    return detail::materialize<F>::from(detail::sqrt_guard<G>(detail::pi_guard_c<G>()));
}

template <class F>
detail::require_float_t<F, const F&>
sqrt_pi() noexcept {
    static const F value = calculate_sqrt_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_inv_sqrt_pi() noexcept {
    using G = detail::guard_of<F>;
    return detail::materialize<F>::from(detail::inv_sqrt_guard<G>(detail::pi_guard_c<G>()));
}

template <class F>
detail::require_float_t<F, const F&>
inv_sqrt_pi() noexcept {
    static const F value = calculate_inv_sqrt_pi<F>();
    return value;
}

template <class F>
detail::require_float_t<F>
calculate_two_inv_sqrt_pi() noexcept {
    using G = detail::guard_of<F>;
    return detail::materialize<F>::from(
        fmath::scalb_i(detail::inv_sqrt_guard<G>(detail::pi_guard_c<G>()), 1)
    );
}

template <class F>
detail::require_float_t<F, const F&>
two_inv_sqrt_pi() noexcept {
    static const F value = calculate_two_inv_sqrt_pi<F>();
    return value;
}

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_CONSTANTS_HPP