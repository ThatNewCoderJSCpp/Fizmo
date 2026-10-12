#ifndef FIZMO_MULTIPRECISION_NORMAL_TRIG_HPP
#define FIZMO_MULTIPRECISION_NORMAL_TRIG_HPP

#include "Details/bigfloat_bridge.hpp"

#include <cmath>

namespace fizmo {
namespace math {

#define FIZMO_DEFINE_TRIG(NAME, NATIVE)                                                                                      \
    template <class T>                                                                                             \
    typename std::enable_if<std::is_integral<T>::value, double>::type                                              \
    NAME(T v) noexcept { const double d = static_cast<double>(v); return NATIVE; }                                 \
                                                                                                                   \
    template <class T>                                                                                             \
    typename std::enable_if<std::is_floating_point<T>::value, T>::type                                             \
    NAME(T v) noexcept { const T d = v; return NATIVE; }                                                           \
                                                                                                                   \
    template <std::size_t B, fizmo::multiprecision::sign S>                                                                               \
    fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>> NAME(const fizmo::multiprecision::integer<B, S>& n) {                                           \
        const BigFloat a(n);                                                                                       \
        return bdetail::eval<fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>>>([&a](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, c); }); \
    }                                                                                                              \
                                                                                                                   \
    template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>                                                              \
    fizmo::multiprecision::floatmp<TB, MB, S> NAME(const fizmo::multiprecision::floatmp<TB, MB, S>& x) {                                                         \
        if (x.is_undefined())                 return fizmo::multiprecision::floatmp<TB, MB, S>::undefined();                              \
        if (x.is_nan() || x.is_infinite())    return fizmo::multiprecision::floatmp<TB, MB, S>::nan();                                    \
        const BigFloat a(x);                                                                                       \
        return bdetail::eval<fizmo::multiprecision::floatmp<TB, MB, S>>([&a](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, c); });          \
    }

FIZMO_DEFINE_TRIG(sin, std::sin(d))
FIZMO_DEFINE_TRIG(cos, std::cos(d))
FIZMO_DEFINE_TRIG(tan, std::tan(d))
FIZMO_DEFINE_TRIG(csc, 1 / std::sin(d))
FIZMO_DEFINE_TRIG(sec, 1 / std::cos(d))
FIZMO_DEFINE_TRIG(cot, 1 / std::tan(d))

#undef FIZMO_DEFINE_TRIG

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_NORMAL_TRIG_HPP
