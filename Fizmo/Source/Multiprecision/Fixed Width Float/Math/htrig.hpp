#ifndef FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_HPP
#define FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_HPP

#include "Details/htrig_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

#define FIZMO_DEFINE_HYP(name, run)                                                   \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, double>::type                 \
    name(T v) noexcept { return hdetail::native_##name(static_cast<double>(v)); }      \
                                                                                      \
    template <class T>                                                                \
    typename std::enable_if<std::is_floating_point<T>::value, T>::type                \
    name(T v) noexcept { return hdetail::native_##name(v); }                           \
                                                                                      \
    template <std::size_t B, sign S>                                                  \
    fizmo_float_from_int_t<integer<B, S>> name(const integer<B, S>& n) noexcept {     \
        using F = fizmo_float_from_int_t<integer<B, S>>;                               \
        if (n.is_undefined()) return F::undefined();                                   \
        return edetail::run(F(typename F::sstore_t(n)));                               \
    }                                                                                 \
                                                                                      \
    template <std::size_t TB, std::size_t MB, sign S>                                 \
    floatmp<TB, MB, S> name(const floatmp<TB, MB, S>& x) noexcept {                    \
        return edetail::run(x);                                                        \
    }

FIZMO_DEFINE_HYP(sinh, sinh_run)
FIZMO_DEFINE_HYP(cosh, cosh_run)
FIZMO_DEFINE_HYP(tanh, tanh_run)
FIZMO_DEFINE_HYP(csch, csch_run)
FIZMO_DEFINE_HYP(sech, sech_run)
FIZMO_DEFINE_HYP(coth, coth_run)

#undef FIZMO_DEFINE_HYP

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_HPP