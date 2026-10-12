#ifndef FIZMO_STATISTICS_COMMON_HPP
#define FIZMO_STATISTICS_COMMON_HPP

#include "../Multiprecision/include.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {
namespace statistics {

using math::BigFloat;
using math::BigFloatContext;
using math::BigInt;
using math::BigUInt;

enum class Tail : std::uint8_t {
    lower = 0,
    upper,
    two,
    left  = lower,
    right = upper
};

struct Condition {
    std::string name;
    bool        met = true;
    std::string detail;
};

inline bool all_met(const std::vector<Condition>& conditions) {
    for (const Condition& c : conditions) {
        if (!c.met) return false;
    }
    return true;
}

namespace sdetail {

template <class T>
struct is_big : std::is_same<typename std::decay<T>::type, BigFloat> {};

template <class T>
struct is_mp_float : math::bdetail::is_mp_float<typename std::decay<T>::type> {};

template <class T>
struct is_real : std::integral_constant<bool, std::is_floating_point<T>::value || is_big<T>::value || is_mp_float<T>::value> {};

template <class T>
inline BigFloat big(const T& v) { return BigFloat(v); }

inline BigFloat big(const BigFloat& v) { return v; }

template <class T>
inline BigFloatContext context_for() {
    if constexpr (is_big<T>::value) {
        return BigFloatContext::current();
    } else if constexpr (std::is_floating_point<T>::value) {
        return BigFloatContext(static_cast<std::size_t>(std::numeric_limits<T>::digits), math::RoundingMode::nearest_even);
    } else {
        return BigFloatContext(T::mantissa_bits + 1, math::RoundingMode::nearest_even);
    }
}

template <class T>
inline T from_big(const BigFloat& v) {
    if constexpr (is_big<T>::value) {
        return v;
    } else if constexpr (std::is_floating_point<T>::value) {
        return v.template to_std_float<T>();
    } else {
        return v.template to_floatmp<T>();
    }
}

template <class T, class Fn>
inline T compute(Fn&& fn) {
    if constexpr (is_big<T>::value) {
        return fn(BigFloatContext::current());
    } else {
        return math::bdetail::eval<T>(std::forward<Fn>(fn));
    }
}

template <class T>
inline T nan_value() { return from_big<T>(BigFloat::nan()); }

template <class T>
inline bool less(const T& a, const T& b) { return BigFloat::compare(big(a), big(b)) == BigFloat::ordering::less; }

template <class C>
inline std::vector<BigFloat> to_big_vector(const C& data) {
    std::vector<BigFloat> out;
    for (const auto& v : data) out.push_back(big(v));
    return out;
}

template <class C>
inline std::vector<std::vector<BigFloat>> to_big_table(const C& table) {
    std::vector<std::vector<BigFloat>> out;
    for (const auto& row : table) out.push_back(to_big_vector(row));
    return out;
}

template <class T>
inline std::vector<T> from_big_vector(const std::vector<BigFloat>& v) {
    std::vector<T> out;
    out.reserve(v.size());
    for (const BigFloat& x : v) out.push_back(from_big<T>(x));
    return out;
}

} // namespace sdetail

} // namespace statistics
} // namespace fizmo

#endif // FIZMO_STATISTICS_COMMON_HPP
