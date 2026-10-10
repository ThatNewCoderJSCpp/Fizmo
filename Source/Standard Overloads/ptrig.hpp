#ifndef FIZMO_CONSTEXPR_PTRIG_HPP
#define FIZMO_CONSTEXPR_PTRIG_HPP

#include <type_traits>
#include <limits>
#include "../Basic/constants.hpp"
#include "abs.hpp"
#include "sqrt.hpp"
#include "pow.hpp"

namespace fizmo {
namespace math {

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
sinp_constexpr(const T x) noexcept {
    const T constant_part = (static_cast<T>(4) - static_cast<T>(3) * x) * static_cast<T>(0.5);
    const T root_part = sqrt_constexpr(static_cast<T>(1) + constant_part * constant_part);
    const T first_part = root_part + constant_part;
    const T second_part = root_part - constant_part;
    return static_cast<T>(3.0) - cbrt_constexpr(first_part) - cbrt_constexpr(second_part);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
sinp_constexpr(const T x) noexcept { return sinp_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
cosp_constexpr(const T x) noexcept {
    const T constant_part = (static_cast<T>(4) - static_cast<T>(3) * x) * static_cast<T>(0.5);
    const T root_part = sqrt_constexpr(static_cast<T>(1) + constant_part * constant_part);
    const T first_part = root_part + constant_part;
    const T second_part = root_part - constant_part;
    return cbrt_constexpr(first_part) - cbrt_constexpr(second_part);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
cosp_constexpr(const T x) noexcept { return cosp_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
tanp_constexpr(const T x) noexcept {
    const T cos_val = cosp_constexpr(x);
    if (abs_constexpr(cos_val) <= 1e-18) { return std::numeric_limits<T>::quiet_NaN(); }
    return sinp_constexpr(x) / cos_val;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
tanp_constexpr(const T x) noexcept { return tanp_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
secp_constexpr(const T x) noexcept {
    const T cos_val = cosp_constexpr(x);
    if (abs_constexpr(cos_val) <= 1e-18) { return std::numeric_limits<T>::quiet_NaN(); }
    return static_cast<T>(1) / cos_val;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
secp_constexpr(const T x) noexcept { return secp_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
cscp_constexpr(const T x) noexcept {
    const T sin_val = sinp_constexpr(x);
    if (abs_constexpr(sin_val) <= 1e-18) { return std::numeric_limits<T>::quiet_NaN(); }
    return static_cast<T>(1) / sin_val;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
cscp_constexpr(const T x) noexcept { return cscp_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
cotp_constexpr(const T x) noexcept {
    const T sin_val = sinp_constexpr(x);
    if (abs_constexpr(sin_val) <= 1e-18) { return std::numeric_limits<T>::quiet_NaN(); }
    return cosp_constexpr(x) / sin_val;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
cotp_constexpr(const T x) noexcept { return cotp_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
arccosp_constexpr(const T x) noexcept { return -static_cast<T>(1.0 / 3.0) * (x * x * x + static_cast<T>(3) * x - static_cast<T>(4)); }

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
arccosp_constexpr(const T x) noexcept { return arccosp_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
arcsinp_constexpr(const T x, int type = 1) noexcept {
    if (type < 1 || type > 6) { type = 1; }
    const T p6 = x * x * x * x * x * x;
    const T p5 = x * x * x * x * x;
    const T p4 = x * x * x * x;
    const T p3 = x * x * x;
    const T p2 = x * x;
    const T FOUR_THIRDS = static_cast<T>(4.0 / 3.0);
    
    const T type_1_2_expr = p6 - static_cast<T>(18) * p5 + static_cast<T>(129) * p4 
                           - static_cast<T>(468) * p3 + static_cast<T>(900) * p2 
                           - static_cast<T>(864) * x + static_cast<T>(320);
    
    const T THREE_ROOT_TWO = static_cast<T>(3.0 * sqrt_constexpr(2.0));
    const T THREE_ROOT_TWO_RECIPROCAL = static_cast<T>(1.0 / THREE_ROOT_TWO);
    const T THREE_ROOT_3 = static_cast<T>(3.0 * sqrt_constexpr(3.0));
    const T CONST_ARG = static_cast<T>(2.0) * p6 - static_cast<T>(36.0) * p5 
                      + static_cast<T>(276.0) * p4 - static_cast<T>(1152.0) * p3 
                      + static_cast<T>(2745.0) * p2 - static_cast<T>(3510.0) * x 
                      + static_cast<T>(1855.0);
    const T CONST_NESTED_ARG = -static_cast<T>(1.0) * pow_constexpr(x - static_cast<T>(3.0), static_cast<T>(4.0)) + pow_constexpr(static_cast<T>(2.0) * p2 - static_cast<T>(12.0) * x + static_cast<T>(21.0), static_cast<T>(2.0));
    const T ROOT_3_CONST_NEST = THREE_ROOT_3 * sqrt_constexpr(CONST_NESTED_ARG);

    switch (type) {
        case 1:
            return FOUR_THIRDS - sqrt_constexpr(type_1_2_expr);
        case 2:
            return FOUR_THIRDS + sqrt_constexpr(type_1_2_expr);
        case 3:
            return FOUR_THIRDS - THREE_ROOT_TWO_RECIPROCAL * sqrt_constexpr(CONST_ARG - ROOT_3_CONST_NEST);
        case 4:
            return FOUR_THIRDS + THREE_ROOT_TWO_RECIPROCAL * sqrt_constexpr(CONST_ARG - ROOT_3_CONST_NEST);
        case 5:
            return FOUR_THIRDS - THREE_ROOT_TWO_RECIPROCAL * sqrt_constexpr(CONST_ARG + ROOT_3_CONST_NEST);
        case 6:
            return FOUR_THIRDS + THREE_ROOT_TWO_RECIPROCAL * sqrt_constexpr(CONST_ARG + ROOT_3_CONST_NEST);
        default:
            return FOUR_THIRDS - sqrt_constexpr(type_1_2_expr); // Type 1 as fallback
    }
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
arcsinp_constexpr(const T x, int type = 1) noexcept { return arcsinp_constexpr(static_cast<double>(x), type); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
arctanp_constexpr(const T x, int value_type = 1, int function_type = 1) noexcept {
    if (function_type < 1 || function_type > 2) { function_type = 1; }
    const T constant_div = sqrt_constexpr(static_cast<T>(1) + x * x);
    if (abs_constexpr(constant_div) <= 1e-18) { return std::numeric_limits<T>::quiet_NaN(); }
    return function_type == 1 ? arcsinp_constexpr(x / constant_div, value_type) : arccosp_constexpr(static_cast<T>(1) / constant_div);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
arctanp_constexpr(const T x, int value_type = 1, int function_type = 1) noexcept { return arctanp_constexpr(static_cast<double>(x), value_type, function_type); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
arccscp_constexpr(const T x, int value_type = 1) noexcept {
    if (abs_constexpr(x) <= 1e-18) { return std::numeric_limits<T>::quiet_NaN(); }
    return arcsinp_constexpr(static_cast<T>(1) / x, value_type);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
arccscp_constexpr(const T x, int value_type = 1) noexcept { return arccscp_constexpr(static_cast<double>(x), value_type); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
arcsecp_constexpr(const T x) noexcept {
    if (abs_constexpr(x) <= 1e-18) { return std::numeric_limits<T>::quiet_NaN(); }
    return arccosp_constexpr(static_cast<T>(1) / x);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
arcsecp_constexpr(const T x) noexcept { return arcsecp_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
arccotp_constexpr(const T x, int value_type = 1, int function_type = 1) noexcept {
    if (abs_constexpr(x) <= 1e-18) { return std::numeric_limits<T>::quiet_NaN(); }
    return arctanp_constexpr(static_cast<T>(1) / x, value_type, function_type);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
arccotp_constexpr(const T x, int value_type = 1, int function_type = 1) noexcept { return arccotp_constexpr(static_cast<double>(x), value_type, function_type); }

} // namespace math
} // namespace fizmo

#endif // FIZMO_CONSTEXPR_PTRIG_HPP