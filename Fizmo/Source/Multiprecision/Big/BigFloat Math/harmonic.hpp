#ifndef FIZMO_MULTIPRECISION_BIG_HARMONIC_HPP
#define FIZMO_MULTIPRECISION_BIG_HARMONIC_HPP

#include "polygamma.hpp"
#include "euler_mascheroni.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace hmdetail {

 void hm_split(std::uint64_t a, std::uint64_t b, BigUInt& T, BigUInt& Q);

 bool hm_exact_ok(std::uint64_t n, std::size_t prec);

 BigFloat hm_exact(std::uint64_t n, const BigFloatContext& ctx);

 bool hm_as_u64(const BigFloat& x, std::uint64_t& n);

inline bool hm_tiny(const BigFloat& x, std::size_t want) {               
    return x.get_exp_base2() < -static_cast<std::int64_t>(want) - 2;
}

 BigFloat hm_plus_one(const BigFloat& x);

 std::size_t hm_cancel(const BigFloat& a, const BigFloat& b, const BigFloat& r);

 bool hm_safe(const BigFloat& v, std::size_t want, std::size_t lost, std::size_t prec);

inline bool hm_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

} // namespace hmdetail

 BigFloat harmonic(const BigFloat& x, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(harmonic)

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline BigFloat harmonic(T x, const BigFloatContext& ctx) { return harmonic(BigFloat(x), ctx); }

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline BigFloat harmonic(T x) { return harmonic(BigFloat(x), BigFloatContext::current()); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_HARMONIC_HPP