#ifndef FIZMO_ABS_HPP
#define FIZMO_ABS_HPP

#include "is_inf_nan.hpp"

namespace fizmo {

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr T abs_constexpr(const T x) noexcept { 
    if (isnan_constexpr(x)) { return x; }
    return x < 0 ? -x : x; 
}

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr T copysign_constexpr(const T mag, const T sgn) noexcept { return signbit_constexpr(sgn) ? -abs_constexpr(mag) : abs_constexpr(mag); }

} // namespace fizmo

#endif // FIZMO_ABS_HPP