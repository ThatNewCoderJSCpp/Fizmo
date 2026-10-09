#ifndef FIZMO_MULTIPRECISION_BIG_GENERALIZED_GAUSSIAN_HPP
#define FIZMO_MULTIPRECISION_BIG_GENERALIZED_GAUSSIAN_HPP

#include "gamma.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace ggdetail {

 BigFloat gg_plus_one(const BigFloat& x);

inline BigUInt gg_pow(BigUInt b, std::uint64_t e) {
    BigUInt r = BigUInt::one();

    while (e != 0) {
        if (e & 1u) r = r * b;
        e >>= 1;
        if (e != 0) b = b * b;
    }

    return r;
}

 bool gg_safe(const BigFloat& v, std::size_t want, double lost, std::size_t prec);

inline bool gg_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

 double gg_units(double nu, double lnb);

 bool gg_integer_nu(const BigFloat& a1, const BigFloat& c, std::uint64_t& n);

 bool gg_exact(std::uint64_t n, const BigFloat& b, const BigFloat& c, const BigFloatContext& ctx, BigFloat& out);

} // namespace ggdetail

 BigFloat generalized_gaussian_moment(const BigFloat& power, const BigFloat& rate, const BigFloat& shape, const BigFloatContext& ctx);

inline BigFloat generalized_gaussian_integral(const BigFloat& rate, const BigFloat& shape, const BigFloatContext& ctx) {
    return generalized_gaussian_moment(BigFloat::zero(), rate, shape, ctx);
}

inline BigFloat generalized_gaussian_moment(const BigFloat& power, const BigFloat& rate, const BigFloat& shape) {
    return generalized_gaussian_moment(power, rate, shape, BigFloatContext::current());
}

inline BigFloat generalized_gaussian_integral(const BigFloat& rate, const BigFloat& shape) {
    return generalized_gaussian_integral(rate, shape, BigFloatContext::current());
}

template <typename P, typename R, typename S, typename std::enable_if<!(std::is_same<P, BigFloat>::value && std::is_same<R, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat generalized_gaussian_moment(const P& power, const R& rate, const S& shape, const BigFloatContext& c) {
    return generalized_gaussian_moment(BigFloat(power), BigFloat(rate), BigFloat(shape), c);
}

template <typename P, typename R, typename S, typename std::enable_if<!(std::is_same<P, BigFloat>::value && std::is_same<R, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat generalized_gaussian_moment(const P& power, const R& rate, const S& shape) {
    return generalized_gaussian_moment(BigFloat(power), BigFloat(rate), BigFloat(shape), BigFloatContext::current());
}

template <typename R, typename S, typename std::enable_if<!(std::is_same<R, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat generalized_gaussian_integral(const R& rate, const S& shape, const BigFloatContext& c) {
    return generalized_gaussian_integral(BigFloat(rate), BigFloat(shape), c);
}

template <typename R, typename S, typename std::enable_if<!(std::is_same<R, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat generalized_gaussian_integral(const R& rate, const S& shape) {
    return generalized_gaussian_integral(BigFloat(rate), BigFloat(shape), BigFloatContext::current());
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_GENERALIZED_GAUSSIAN_HPP