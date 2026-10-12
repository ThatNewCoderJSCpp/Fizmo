#ifndef FIZMO_MULTIPRECISION_BIG_INCOMPLETE_HPP
#define FIZMO_MULTIPRECISION_BIG_INCOMPLETE_HPP

#include "gamma.hpp"

#include <cstdint>
#include <type_traits>

namespace fizmo {
namespace math {

BigFloat log1p(const BigFloat& x, const BigFloatContext& ctx);
BigFloat expm1(const BigFloat& x, const BigFloatContext& ctx);

BigFloat fibonacci(const BigFloat& x, const BigFloatContext& ctx);
BigFloat lucas(const BigFloat& x, const BigFloatContext& ctx);

BigFloat regularized_gamma_p(const BigFloat& a, const BigFloat& x, const BigFloatContext& ctx);
BigFloat regularized_gamma_q(const BigFloat& a, const BigFloat& x, const BigFloatContext& ctx);
BigFloat lower_incomplete_gamma(const BigFloat& a, const BigFloat& x, const BigFloatContext& ctx);
BigFloat upper_incomplete_gamma(const BigFloat& a, const BigFloat& x, const BigFloatContext& ctx);
BigFloat inverse_regularized_gamma_p(const BigFloat& a, const BigFloat& p, const BigFloatContext& ctx);
BigFloat inverse_regularized_gamma_q(const BigFloat& a, const BigFloat& q, const BigFloatContext& ctx);

BigFloat regularized_beta(const BigFloat& x, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);
BigFloat regularized_beta_complement(const BigFloat& x, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);
BigFloat incomplete_beta(const BigFloat& x, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);
BigFloat inverse_regularized_beta(const BigFloat& p, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);
BigFloat inverse_regularized_beta_complement(const BigFloat& q, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);

inline BigFloat log1p(const BigFloat& x)     { return log1p(x, BigFloatContext::current()); }
inline BigFloat expm1(const BigFloat& x)     { return expm1(x, BigFloatContext::current()); }
inline BigFloat fibonacci(const BigFloat& x) { return fibonacci(x, BigFloatContext::current()); }
inline BigFloat lucas(const BigFloat& x)     { return lucas(x, BigFloatContext::current()); }

namespace igdetail {

template <class T>
struct operand : std::integral_constant<bool, std::is_arithmetic<T>::value || std::is_same<T, BigInt>::value
                                              || std::is_same<T, BigUInt>::value || std::is_same<T, BigFloat>::value> {};

template <class... T> struct all_operands;
template <> struct all_operands<> : std::true_type {};
template <class T, class... R>
struct all_operands<T, R...> : std::integral_constant<bool, operand<typename std::decay<T>::type>::value && all_operands<R...>::value> {};

template <class... T> struct any_not_big;
template <> struct any_not_big<> : std::false_type {};
template <class T, class... R>
struct any_not_big<T, R...> : std::integral_constant<bool, !std::is_same<typename std::decay<T>::type, BigFloat>::value || any_not_big<R...>::value> {};

template <class... T>
using enable_forward = typename std::enable_if<all_operands<T...>::value && any_not_big<T...>::value, int>::type;

template <class... T>
using enable_all = typename std::enable_if<all_operands<T...>::value, int>::type;

} // namespace igdetail

#define FIZMO_MP_INCOMPLETE_UNARY(FN)                                                                              \
    template <class X, igdetail::enable_forward<X> = 0>                                                            \
    inline BigFloat FN(const X& x, const BigFloatContext& c) { return FN(BigFloat(x), c); }                        \
    template <class X, typename std::enable_if<std::is_arithmetic<X>::value || std::is_same<X, BigInt>::value       \
                                               || std::is_same<X, BigUInt>::value, int>::type = 0>                 \
    inline BigFloat FN(const X& x) { return FN(BigFloat(x), BigFloatContext::current()); }

#define FIZMO_MP_INCOMPLETE_BINARY(FN)                                                                             \
    template <class A, class B, igdetail::enable_forward<A, B> = 0>                                                \
    inline BigFloat FN(const A& a, const B& b, const BigFloatContext& c) { return FN(BigFloat(a), BigFloat(b), c); } \
    template <class A, class B, igdetail::enable_all<A, B> = 0>                                                    \
    inline BigFloat FN(const A& a, const B& b) { return FN(BigFloat(a), BigFloat(b), BigFloatContext::current()); }

#define FIZMO_MP_INCOMPLETE_TERNARY(FN)                                                                            \
    template <class X, class A, class B, igdetail::enable_forward<X, A, B> = 0>                                    \
    inline BigFloat FN(const X& x, const A& a, const B& b, const BigFloatContext& c) {                             \
        return FN(BigFloat(x), BigFloat(a), BigFloat(b), c);                                                       \
    }                                                                                                              \
    template <class X, class A, class B, igdetail::enable_all<X, A, B> = 0>                                        \
    inline BigFloat FN(const X& x, const A& a, const B& b) {                                                       \
        return FN(BigFloat(x), BigFloat(a), BigFloat(b), BigFloatContext::current());                              \
    }

FIZMO_MP_INCOMPLETE_UNARY(log1p)
FIZMO_MP_INCOMPLETE_UNARY(expm1)

FIZMO_MP_INCOMPLETE_BINARY(regularized_gamma_p)
FIZMO_MP_INCOMPLETE_BINARY(regularized_gamma_q)
FIZMO_MP_INCOMPLETE_BINARY(lower_incomplete_gamma)
FIZMO_MP_INCOMPLETE_BINARY(upper_incomplete_gamma)
FIZMO_MP_INCOMPLETE_BINARY(inverse_regularized_gamma_p)
FIZMO_MP_INCOMPLETE_BINARY(inverse_regularized_gamma_q)

FIZMO_MP_INCOMPLETE_TERNARY(regularized_beta)
FIZMO_MP_INCOMPLETE_TERNARY(regularized_beta_complement)
FIZMO_MP_INCOMPLETE_TERNARY(incomplete_beta)
FIZMO_MP_INCOMPLETE_TERNARY(inverse_regularized_beta)
FIZMO_MP_INCOMPLETE_TERNARY(inverse_regularized_beta_complement)

#undef FIZMO_MP_INCOMPLETE_UNARY
#undef FIZMO_MP_INCOMPLETE_BINARY
#undef FIZMO_MP_INCOMPLETE_TERNARY

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_INCOMPLETE_HPP
