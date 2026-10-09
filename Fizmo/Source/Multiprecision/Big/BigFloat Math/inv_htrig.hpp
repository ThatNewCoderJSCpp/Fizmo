#ifndef FIZMO_MULTIPRECISION_BIG_INVERSE_HYPERBOLIC_HPP
#define FIZMO_MULTIPRECISION_BIG_INVERSE_HYPERBOLIC_HPP

#include "inv_trig.hpp"
#include "logarithms.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

 BigFloat atanh_core(const BigFloat& u, const BigFloatContext& wc, std::size_t& lost);

 BigFloat asinh_raw(const BigFloat& x, const BigFloatContext& wc, std::size_t& lost);

 BigFloat acosh_raw(const BigFloat& x, const BigFloatContext& wc, std::size_t& lost);

enum class ih_sel : std::uint8_t { asinh_v, acosh_v, atanh_v, acoth_v, asech_v, acsch_v };

 BigFloat ih_core(ih_sel sel, const BigFloat& x, const BigFloatContext& wc, std::size_t& lost);

 BigFloat ih_dispatch(ih_sel sel, const BigFloat& x, const BigFloatContext& ctx);

} // namespace detail

 BigFloat arcsinh(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat arccosh(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat arctanh(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat arccoth(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat arcsech(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat arccsch(const BigFloat& x, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(arcsinh)
FIZMO_MP_TRIG_FORWARD(arccosh)
FIZMO_MP_TRIG_FORWARD(arctanh)
FIZMO_MP_TRIG_FORWARD(arccoth)
FIZMO_MP_TRIG_FORWARD(arcsech)
FIZMO_MP_TRIG_FORWARD(arccsch)

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_INVERSE_HYPERBOLIC_HPP