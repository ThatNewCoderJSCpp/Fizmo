#ifndef TRIANGULAR_NUMBERS_HPP
#define TRIANGULAR_NUMBERS_HPP

#include "../Standard Overloads/sqrt.hpp"

namespace fizmo {
namespace math {

template <typename T>
constexpr typename std::enable_if<std::is_arithmetic<T>::value, typename std::common_type<double, T>::type>::type
triangular_number(const T n) noexcept {
    using CT = typename std::common_type<double, T>::type;
    return 0.5 * static_cast<CT>(n) * (static_cast<CT>(n) + 1);
}

template <typename T>
constexpr typename std::enable_if<std::is_arithmetic<T>::value, std::pair<typename std::common_type<double, T>::type, typename std::common_type<double, T>::type>>::type
triangular_root(const T x) noexcept {
    using CT = typename std::common_type<double, T>::type;
    if (x < -0.125) { return std::make_pair(fizmo::constants::QUIET_NAN<CT>, fizmo::constants::QUIET_NAN<CT>); }
    const CT princ = 0.5 * (-1 + fizmo::math::sqrt_constexpr(1 + 8 * static_cast<CT>(x)));
    return std::make_pair(princ, -princ - 1);
}

template <typename T>
constexpr typename std::enable_if<std::is_arithmetic<T>::value, typename std::common_type<double, T>::type>::type
principal_triangualar_root(const T x) noexcept { return triangular_root(x).first; }

} // namespace math
} // namespace fizmo

#endif // TRIANGULAR_NUMBERS_HPP