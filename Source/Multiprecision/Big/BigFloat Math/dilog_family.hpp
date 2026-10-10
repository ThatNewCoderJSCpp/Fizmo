#ifndef FIZMO_MULTIPRECISION_BIG_DILOG_FAMILY_HPP
#define FIZMO_MULTIPRECISION_BIG_DILOG_FAMILY_HPP

#include "polylog.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace rgdetail {

using ztdetail::zt_val;
using hzdetail::hz_l2;
using hzdetail::hz_xadd;

inline zt_val rg_combine_units(const BigFloat& v, double units) {
    if (v.is_zero()) return zt_val{v, 1.0e9};
    return zt_val{v, std::log2(units) + 1.0};
}

zt_val rg_low(const BigFloat& x, std::size_t want);

zt_val rg_high(const BigFloat& x, std::size_t want);

zt_val rg_raw(const BigFloat& x, std::size_t want);

std::size_t rg_guard0(const BigFloat& x);

bool rg_normalized_exact(const BigFloat& x, BigFloat& out);

} // namespace rgdetail

BigFloat rogers_l(const BigFloat& x, const BigFloatContext& ctx);

BigFloat rogers_l_normalized(const BigFloat& x, const BigFloatContext& ctx);

BigFloat spence_function(const BigFloat& x, const BigFloatContext& ctx);

BigFloat spence_integral(const BigFloat& x, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(rogers_l)
FIZMO_MP_TRIG_FORWARD(rogers_l_normalized)
FIZMO_MP_TRIG_FORWARD(spence_function)
FIZMO_MP_TRIG_FORWARD(spence_integral)

#define FIZMO_MP_DILOGF_ARITH_FORWARD(FN)                                                      \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0> \
    inline BigFloat FN(T x, const BigFloatContext& c) { return FN(BigFloat(x), c); }            \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0> \
    inline BigFloat FN(T x) { return FN(BigFloat(x), BigFloatContext::current()); }

FIZMO_MP_DILOGF_ARITH_FORWARD(rogers_l)
FIZMO_MP_DILOGF_ARITH_FORWARD(rogers_l_normalized)
FIZMO_MP_DILOGF_ARITH_FORWARD(spence_function)
FIZMO_MP_DILOGF_ARITH_FORWARD(spence_integral)

#undef FIZMO_MP_DILOGF_ARITH_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_DILOG_FAMILY_HPP