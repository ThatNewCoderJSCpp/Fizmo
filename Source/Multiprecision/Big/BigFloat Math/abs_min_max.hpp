#ifndef FIZMO_MULTIPRECISION_BIG_ABS_MIN_MAX_HPP
#define FIZMO_MULTIPRECISION_BIG_ABS_MIN_MAX_HPP

#include "../big_float.hpp"

#include <initializer_list>

namespace fizmo {
namespace multiprecision {
namespace math {

inline BigFloat abs(const BigFloat& x) { return x.abs(); }

BigFloat min(const BigFloat& a, const BigFloat& b);

BigFloat max(const BigFloat& a, const BigFloat& b);

BigFloat min(std::initializer_list<BigFloat> values);

BigFloat max(std::initializer_list<BigFloat> values);

template <typename... Args>
inline BigFloat min(const BigFloat& a, const BigFloat& b, const Args&... args) {
    BigFloat result = min(a, b);

    (void)std::initializer_list<int>{
        ((result = min(result, args)), 0)...
    };

    return result;
}

template <typename... Args>
inline BigFloat max(const BigFloat& a, const BigFloat& b, const Args&... args) {
    BigFloat result = max(a, b);

    (void)std::initializer_list<int>{
        ((result = max(result, args)), 0)...
    };

    return result;
}


BigFloat clamp(
    const BigFloat& x,
    const BigFloat& lo,
    const BigFloat& hi
);

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_ABS_MIN_MAX_HPP
