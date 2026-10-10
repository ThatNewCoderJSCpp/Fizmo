#ifndef ALTERNATING_POWER_FUNCTION_HPP
#define ALTERNATING_POWER_FUNCTION_HPP

#include <type_traits>
#include <limits>
#include "../Standard Overloads/abs.hpp"
#include "../Basic/constants.hpp"

namespace fizmo {
namespace math {

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, int>::type 
alternating_power(const T x) noexcept { return (x == 0) ? 1 : ((x & 1) ? -1 : 1); }

} // namespace math
} // namespace fizmo

#endif // ALTERNATING_POWER_FUNCTION_HPP