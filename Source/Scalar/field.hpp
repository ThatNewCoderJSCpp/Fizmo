#ifndef FIZMO_SCALAR_FIELD_HPP
#define FIZMO_SCALAR_FIELD_HPP

#include "scalar.hpp"

namespace fizmo {
namespace scalar {

struct NativeContext {};

template <class S, class = void>
struct field;

template <class U>
struct is_mixed_scalar : std::integral_constant<bool, std::is_arithmetic<U>::value || is_fixed_width<U>::value> {};

template <class T>
struct field<T, typename std::enable_if<is_real<T>::value>::type> {
    using value_type   = T;
    using context_type = NativeContext;
    using target_type  = Target<T>;

    static constexpr bool is_big = false;

    static context_type default_context() noexcept { return {}; }
    static target_type  target(const context_type&) noexcept { return {}; }
    static std::size_t  bits(const context_type&) noexcept { return digits<T>(); }

    static T zero() { return T(0); }
    static T one() { return T(1); }
    static T nan() { return scalar::nan<T>(); }
    static T infinity(bool negative = false) { return scalar::infinity<T>(negative); }
    static T default_epsilon() { return scalar::epsilon<T>(); }

    static BigFloat big(const T& x) { return BigFloat(x); }
    static T round(const context_type&, const BigFloat& v) { return from_big<T>(v); }

    template <class U, typename std::enable_if<std::is_same<U, T>::value, int>::type = 0>
    static T from(const U& u) { return u; }

    template <class U, typename std::enable_if<!std::is_same<U, T>::value && std::is_arithmetic<U>::value && std::is_floating_point<T>::value, int>::type = 0>
    static T from(const U& u) { return static_cast<T>(u); }

    template <class U, typename std::enable_if<!std::is_same<U, T>::value && is_mixed_scalar<U>::value && !(std::is_arithmetic<U>::value && std::is_floating_point<T>::value), int>::type = 0>
    static T from(const U& u) { return from_big<T>(BigFloat(u)); }

    template <class U>
    static bool exact_convert(const U& b, T& out) {
        if constexpr (std::is_same<U, T>::value) {
            out = b;
            return true;
        } else if constexpr (std::is_integral<U>::value && std::is_floating_point<T>::value) {
            out = static_cast<T>(b);
            const long double lim = std::ldexp(1.0L, std::numeric_limits<T>::digits);
            return static_cast<long double>(b) <= lim && static_cast<long double>(b) >= -lim;
        } else if constexpr (std::is_floating_point<U>::value && std::is_floating_point<T>::value) {
            out = static_cast<T>(b);
            if (!std::isfinite(b)) return true;
            return std::isfinite(out) && static_cast<U>(out) == b;
        } else {
            return fits(BigFloat(b), out);
        }
    }

    static bool fits(const BigFloat& v, T& out) {
        if (!v.is_finite()) {
            out = from_big<T>(v);
            return true;
        }

        out = from_big<T>(v);
        return is_finite(out) && BigFloat::compare(BigFloat(out), v) == BigFloat::ordering::equal;
    }

    static T add(const T& a, const T& b, const context_type& = {}) { return a + b; }
    static T sub(const T& a, const T& b, const context_type& = {}) { return a - b; }
    static T mul(const T& a, const T& b, const context_type& = {}) { return a * b; }
    static T div(const T& a, const T& b, const context_type& = {}) { return a / b; }
    static T neg(const T& a) { return -a; }
    static T abs(const T& a) { return scalar::abs(a); }
    static T sqrt(const T& a, const context_type& = {}) { return scalar::sqrt(a); }

    template <class U>
    static T mul_mixed(const T& a, const U& b, const context_type& = {}) {
        T bt;
        if (exact_convert(b, bt)) return a * bt;
        if (!is_finite(a)) return a * bt;
        return from_big<T>(exact_mul(BigFloat(a), BigFloat(b)));
    }

    template <class U>
    static T div_mixed(const T& a, const U& b, const context_type& c = {}) {
        T bt;
        if (exact_convert(b, bt)) return a / bt;
        if (!is_finite(a)) return a / bt;
        return ratio(BigFloat(a), BigFloat(b), c);
    }

    static T ratio(const BigFloat& num, const BigFloat& den, const context_type& = {}) {
        T n;
        T d;
        if (fits(num, n) && fits(den, d)) return n / d;
        if (!num.is_finite() || !den.is_finite() || den.is_zero()) return from_big<T>(BigFloat::div(num, den, context(64)));
        return ziv(Target<T>(), [&](const BigFloatContext& c) { return BigFloat::div(num, den, c); });
    }

    static T dot(const T* a, const T* b, std::size_t n, const context_type& = {}) { return scalar::dot(a, b, n); }
    static T sum(const T* a, std::size_t n, const context_type& = {}) { return scalar::sum(a, n); }
    static T norm(const T* a, std::size_t n, const context_type& = {}) { return scalar::norm(a, n); }
    static T diff_of_products(const T& a, const T& b, const T& c, const T& d, const context_type& = {}) { return scalar::diff_of_products(a, b, c, d); }
    static bool fast_unit(const T* a, std::size_t n, T* out, const context_type& = {}) { return detail::fast_unit(a, n, out); }
    static T sum_of_products(const T& a, const T& b, const T& c, const T& d, const context_type& = {}) { return scalar::sum_of_products(a, b, c, d); }

    template <class F>
    static T eval(const context_type&, F&& f) { return ziv(Target<T>(), std::forward<F>(f)); }

    template <std::size_t M, class F>
    static std::array<T, M> eval_n(const context_type&, F&& f) { return ziv_array<Target<T>, M>(Target<T>(), std::forward<F>(f)); }

    template <class F>
    static std::vector<T> eval_vec(const context_type&, std::size_t count, F&& f) { return ziv_vector(Target<T>(), count, std::forward<F>(f)); }

    static bool is_nan(const T& x) { return scalar::is_nan(x); }
    static bool is_inf(const T& x) { return scalar::is_inf(x); }
    static bool is_finite(const T& x) { return scalar::is_finite(x); }
    static bool is_zero(const T& x) { return x == T(0); }
    static bool signbit(const T& x) { return scalar::signbit(x); }
    static bool identical(const T& a, const T& b) { return scalar::identical(a, b); }
    static std::string to_string(const T& x) { return scalar::to_string(x); }
};

template <>
struct field<BigFloat, void> {
    using value_type   = BigFloat;
    using context_type = BigFloatContext;
    using target_type  = BigTarget;

    static constexpr bool is_big = true;

    static context_type default_context() noexcept { return BigFloatContext::current(); }
    static target_type  target(const context_type& c) { return BigTarget(c); }
    static std::size_t  bits(const context_type& c) noexcept { return c.precision; }

    static BigFloat zero() { return BigFloat::zero(); }
    static BigFloat one() { return BigFloat::one(); }
    static BigFloat nan() { return BigFloat::nan(); }
    static BigFloat infinity(bool negative = false) { return BigFloat::infinity(negative); }
    static BigFloat default_epsilon() { return BigFloat::ldexp(BigFloat::one(), 1 - static_cast<std::int64_t>(BigFloatContext::current().precision)); }

    static const BigFloat& big(const BigFloat& x) { return x; }
    static BigFloat round(const context_type& c, const BigFloat& v) { return v.rounded(c); }

    static BigFloat from(const BigFloat& u) { return u; }

    template <class U, typename std::enable_if<is_mixed_scalar<U>::value, int>::type = 0>
    static BigFloat from(const U& u) { return BigFloat(u); }

    static BigFloat add(const BigFloat& a, const BigFloat& b, const context_type& c = default_context()) { return BigFloat::add(a, b, c); }
    static BigFloat sub(const BigFloat& a, const BigFloat& b, const context_type& c = default_context()) { return BigFloat::sub(a, b, c); }
    static BigFloat mul(const BigFloat& a, const BigFloat& b, const context_type& c = default_context()) { return BigFloat::mul(a, b, c); }
    static BigFloat div(const BigFloat& a, const BigFloat& b, const context_type& c = default_context()) { return BigFloat::div(a, b, c); }
    static BigFloat neg(const BigFloat& a) { return -a; }
    static BigFloat abs(const BigFloat& a) { return a.abs(); }
    static BigFloat sqrt(const BigFloat& a, const context_type& c = default_context()) { return math::sqrt(a, c); }

    template <class U>
    static BigFloat mul_mixed(const BigFloat& a, const U& b, const context_type& c = default_context()) { return BigFloat::mul(a, from(b), c); }

    template <class U>
    static BigFloat div_mixed(const BigFloat& a, const U& b, const context_type& c = default_context()) { return BigFloat::div(a, from(b), c); }

    static BigFloat ratio(const BigFloat& num, const BigFloat& den, const context_type& c = default_context()) { return BigFloat::div(num, den, c); }

    static BigFloat dot(const BigFloat* a, const BigFloat* b, std::size_t n, const context_type& c = default_context()) { return scalar::dot(a, b, n, c); }
    static BigFloat sum(const BigFloat* a, std::size_t n, const context_type& c = default_context()) { return scalar::sum(a, n, c); }
    static BigFloat norm(const BigFloat* a, std::size_t n, const context_type& c = default_context()) { return scalar::norm(a, n, c); }

    static BigFloat diff_of_products(const BigFloat& a, const BigFloat& b, const BigFloat& x, const BigFloat& y, const context_type& c = default_context()) {
        return scalar::diff_of_products(a, b, x, y, c);
    }

    static bool fast_unit(const BigFloat*, std::size_t, BigFloat*, const context_type& = default_context()) { return false; }

    static BigFloat sum_of_products(const BigFloat& a, const BigFloat& b, const BigFloat& x, const BigFloat& y, const context_type& c = default_context()) {
        return exact_add(exact_mul(a, b), exact_mul(x, y)).rounded(c);
    }

    template <class F>
    static BigFloat eval(const context_type& c, F&& f) { return ziv(BigTarget(c), std::forward<F>(f)); }

    template <std::size_t M, class F>
    static std::array<BigFloat, M> eval_n(const context_type& c, F&& f) { return ziv_array<BigTarget, M>(BigTarget(c), std::forward<F>(f)); }

    template <class F>
    static std::vector<BigFloat> eval_vec(const context_type& c, std::size_t count, F&& f) { return ziv_vector(BigTarget(c), count, std::forward<F>(f)); }

    static bool is_nan(const BigFloat& x) { return x.is_nan() || x.is_undefined(); }
    static bool is_inf(const BigFloat& x) { return x.is_infinite(); }
    static bool is_finite(const BigFloat& x) { return x.is_finite(); }
    static bool is_zero(const BigFloat& x) { return x.is_zero(); }
    static bool signbit(const BigFloat& x) { return x.signbit(); }
    static bool identical(const BigFloat& a, const BigFloat& b) { return scalar::identical(a, b); }
    static std::string to_string(const BigFloat& x) { return x.to_string(); }
};

template <class S>
using context_of = typename field<S>::context_type;

} // namespace scalar
} // namespace fizmo

#endif // FIZMO_SCALAR_FIELD_HPP
