#ifndef FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_HPP
#define FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_HPP

#include "Details/bigfloat_bridge.hpp"

#include <cmath>

namespace fizmo {
namespace math {

#define FIZMO_DEFINE_HYP(NAME, NATIVE)                                                                                      \
    template <class T>                                                                                             \
    typename std::enable_if<std::is_integral<T>::value, double>::type                                              \
    NAME(T v) noexcept { const double d = static_cast<double>(v); return NATIVE; }                                 \
                                                                                                                   \
    template <class T>                                                                                             \
    typename std::enable_if<std::is_floating_point<T>::value, T>::type                                             \
    NAME(T v) noexcept { const T d = v; return NATIVE; }                                                           \
                                                                                                                   \
    template <std::size_t B, sign S>                                                                               \
    fizmo_float_from_int_t<integer<B, S>> NAME(const integer<B, S>& n) {                                           \
        const BigFloat a(n);                                                                                       \
        return bdetail::eval<fizmo_float_from_int_t<integer<B, S>>>([&a](const BigFloatContext& c) { return NAME(a, c); }); \
    }                                                                                                              \
                                                                                                                   \
    template <std::size_t TB, std::size_t MB, sign S>                                                              \
    floatmp<TB, MB, S> NAME(const floatmp<TB, MB, S>& x) {                                                         \
        const BigFloat a(x);                                                                                       \
        return bdetail::eval<floatmp<TB, MB, S>>([&a](const BigFloatContext& c) { return NAME(a, c); });          \
    }

FIZMO_DEFINE_HYP(sinh, std::sinh(d))
FIZMO_DEFINE_HYP(cosh, std::cosh(d))
FIZMO_DEFINE_HYP(tanh, std::tanh(d))
FIZMO_DEFINE_HYP(csch, 1 / std::sinh(d))
FIZMO_DEFINE_HYP(sech, 1 / std::cosh(d))
FIZMO_DEFINE_HYP(coth, 1 / std::tanh(d))

#undef FIZMO_DEFINE_HYP

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_HPP
