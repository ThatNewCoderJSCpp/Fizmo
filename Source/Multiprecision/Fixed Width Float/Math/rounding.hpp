#ifndef FIZMO_MULTIPRECISION_FIXED_ROUNDING_HPP
#define FIZMO_MULTIPRECISION_FIXED_ROUNDING_HPP

#include "Details/bigfloat_bridge.hpp"
#include "../../Big/BigFloat Math/num_theory.hpp"

namespace fizmo {
namespace math {

#define FIZMO_FW_ROUNDING(NAME)                                                                                       \
    template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>                                         \
    fizmo::multiprecision::floatmp<TB, MB, S> NAME(const fizmo::multiprecision::floatmp<TB, MB, S>& x) {             \
        if (!x.is_finite() || x.is_zero()) return x;                                                                 \
        return static_cast<fizmo::multiprecision::floatmp<TB, MB, S>>(NAME(fizmo::multiprecision::BigFloat(x)));     \
    }

FIZMO_FW_ROUNDING(floor)
FIZMO_FW_ROUNDING(ceiling)
FIZMO_FW_ROUNDING(round)
FIZMO_FW_ROUNDING(trunc)

#undef FIZMO_FW_ROUNDING

template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>
fizmo::multiprecision::floatmp<TB, MB, S> fractional_part(const fizmo::multiprecision::floatmp<TB, MB, S>& x) {
    if (x.is_unordered()) return x;
    if (x.is_infinite()) return fizmo::multiprecision::floatmp<TB, MB, S>::undefined();
    return x.get_fractional_part();
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FIXED_ROUNDING_HPP
