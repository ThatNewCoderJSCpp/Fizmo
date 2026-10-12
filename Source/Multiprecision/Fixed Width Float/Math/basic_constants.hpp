#ifndef FIZMO_MULTIPRECISION_CONSTANTS_HPP
#define FIZMO_MULTIPRECISION_CONSTANTS_HPP

#include "Details/basic_const_computing.hpp"
#include "Details/bigfloat_bridge.hpp"

namespace fizmo {
namespace multiprecision {
namespace constants {

#define FIZMO_FW_BASIC_CONSTANT(NAME, REQUIRE)                                                                     \
    template <class F>                                                                                             \
    REQUIRE<F>                                                                                                     \
    calculate_##NAME() {                                                                                           \
        return math::bdetail::eval<F>([](const BigFloatContext& c) { return NAME(c); });                          \
    }                                                                                                              \
                                                                                                                   \
    template <class F>                                                                                             \
    REQUIRE<F, const F&>                                                                                           \
    NAME() {                                                                                                       \
        static const F value = calculate_##NAME<F>();                                                              \
        return value;                                                                                              \
    }

FIZMO_FW_BASIC_CONSTANT(sqrt2, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_sqrt2, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(sqrt3, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_sqrt3, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(sqrt5, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_sqrt5, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(ln2, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_ln2, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(ln10, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_ln10, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(e, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_e, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(phi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_phi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(psi, detail::require_signed_float_t)
FIZMO_FW_BASIC_CONSTANT(pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(two_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(half_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(third_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(quarter_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(sixth_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(pi_180, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(two_inv_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_two_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_pi_180, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(zeta2, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(apery, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(sqrt_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(inv_sqrt_pi, detail::require_float_t)
FIZMO_FW_BASIC_CONSTANT(two_inv_sqrt_pi, detail::require_float_t)

#undef FIZMO_FW_BASIC_CONSTANT

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_CONSTANTS_HPP
