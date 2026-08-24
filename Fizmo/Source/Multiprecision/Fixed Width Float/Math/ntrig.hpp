#ifndef FIZMO_MULTIPRECISION_NORMAL_TRIG_HPP
#define FIZMO_MULTIPRECISION_NORMAL_TRIG_HPP

#include "Details/ntrig_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

#define FIZMO_DEFINE_TRIG(name, run)                                                  \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, double>::type                 \
    name(T v) noexcept { return tdetail::native_##name(static_cast<double>(v)); }      \
                                                                                      \
    template <class T>                                                                \
    typename std::enable_if<std::is_floating_point<T>::value, T>::type                \
    name(T v) noexcept { return tdetail::native_##name(v); }                           \
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

FIZMO_DEFINE_TRIG(sin, sin_run)
FIZMO_DEFINE_TRIG(cos, cos_run)
FIZMO_DEFINE_TRIG(tan, tan_run)
FIZMO_DEFINE_TRIG(csc, csc_run)
FIZMO_DEFINE_TRIG(sec, sec_run)
FIZMO_DEFINE_TRIG(cot, cot_run)

#undef FIZMO_DEFINE_TRIG

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_NORMAL_TRIG_HPP