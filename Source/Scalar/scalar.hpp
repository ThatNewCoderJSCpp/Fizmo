#ifndef FIZMO_SCALAR_HPP
#define FIZMO_SCALAR_HPP

#include "../Multiprecision/include.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace scalar {

using multiprecision::BigFloat;
using multiprecision::BigFloatContext;
using multiprecision::BigUInt;
using multiprecision::RoundingMode;

template <class T>
struct is_fixed_width : std::integral_constant<bool, is_fizmo_float_v<T> && is_fizmo_signed_v<T>> {};

template <class T>
struct is_real : std::integral_constant<bool, std::is_floating_point<T>::value || is_fixed_width<T>::value> {};

template <class T>
using if_real = typename std::enable_if<is_real<T>::value, int>::type;

template <class T>
using if_std = typename std::enable_if<std::is_floating_point<T>::value, int>::type;

template <class T>
using if_fixed = typename std::enable_if<is_fixed_width<T>::value, int>::type;

template <class T, if_std<T> = 0>
constexpr std::size_t digits() noexcept { return static_cast<std::size_t>(std::numeric_limits<T>::digits); }

template <class T, if_fixed<T> = 0>
constexpr std::size_t digits() noexcept { return static_cast<std::size_t>(T::mantissa_bits) + 1; }

inline BigFloatContext context(std::size_t precision) {
    return BigFloatContext(BigFloatContext::clamp_precision(precision), RoundingMode::nearest_even);
}

inline BigFloatContext exact() { return BigFloatContext(BigFloatContext::max_prec, RoundingMode::nearest_even); }

template <class T, if_real<T> = 0>
BigFloat to_big(const T& x) { return BigFloat(x); }

template <class T, if_std<T> = 0>
T from_big(const BigFloat& v) { return v.template to_std_float<T>(); }

template <class T, if_fixed<T> = 0>
T from_big(const BigFloat& v) { return v.template to_floatmp<T>(); }

template <class T, if_std<T> = 0>
bool is_nan(const T& x) noexcept { return std::isnan(x); }

template <class T, if_fixed<T> = 0>
bool is_nan(const T& x) noexcept { return x.is_unordered(); }

template <class T, if_std<T> = 0>
bool is_inf(const T& x) noexcept { return std::isinf(x); }

template <class T, if_fixed<T> = 0>
bool is_inf(const T& x) noexcept { return x.is_infinite(); }

template <class T, if_std<T> = 0>
bool is_finite(const T& x) noexcept { return std::isfinite(x); }

template <class T, if_fixed<T> = 0>
bool is_finite(const T& x) noexcept { return x.is_finite(); }

template <class T, if_std<T> = 0>
bool signbit(const T& x) noexcept { return std::signbit(x); }

template <class T, if_fixed<T> = 0>
bool signbit(const T& x) noexcept { return x.is_negative(); }

template <class T, if_std<T> = 0>
T nan() noexcept { return std::numeric_limits<T>::quiet_NaN(); }

template <class T, if_fixed<T> = 0>
T nan() noexcept { return T::nan(); }

template <class T, if_std<T> = 0>
T infinity(bool negative = false) noexcept { return negative ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity(); }

template <class T, if_fixed<T> = 0>
T infinity(bool negative = false) noexcept { return T::infinity(negative); }

template <class T, if_std<T> = 0>
T epsilon() noexcept { return std::numeric_limits<T>::epsilon(); }

template <class T, if_fixed<T> = 0>
T epsilon() noexcept { return T::epsilon(); }

template <class T, if_std<T> = 0>
bool identical(const T& a, const T& b) noexcept {
    if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
    return a == b && std::signbit(a) == std::signbit(b);
}

template <class T, if_fixed<T> = 0>
bool identical(const T& a, const T& b) noexcept {
    if (is_nan(a) || is_nan(b)) return is_nan(a) && is_nan(b);
    return a == b && a.is_negative() == b.is_negative();
}

inline bool identical(const BigFloat& a, const BigFloat& b) {
    if (!a.is_finite() || !b.is_finite()) {
        if (a.is_nan() != b.is_nan() || a.is_undefined() != b.is_undefined() || a.is_indeterminate() != b.is_indeterminate()) return false;
        if (a.is_unordered()) return true;
    }

    return BigFloat::compare(a, b) == BigFloat::ordering::equal && a.signbit() == b.signbit();
}

template <class T, if_std<T> = 0> T abs(const T& x) { return std::fabs(x); }
template <class T, if_fixed<T> = 0> T abs(const T& x) { return math::abs(x); }
template <class T, if_std<T> = 0> T sqrt(const T& x) { return std::sqrt(x); }
template <class T, if_fixed<T> = 0> T sqrt(const T& x) { return math::sqrt(x); }
template <class T, if_std<T> = 0> T sin(const T& x) { return std::sin(x); }
template <class T, if_fixed<T> = 0> T sin(const T& x) { return math::sin(x); }
template <class T, if_std<T> = 0> T cos(const T& x) { return std::cos(x); }
template <class T, if_fixed<T> = 0> T cos(const T& x) { return math::cos(x); }
template <class T, if_std<T> = 0> T exp(const T& x) { return std::exp(x); }
template <class T, if_fixed<T> = 0> T exp(const T& x) { return math::exp(x); }
template <class T, if_std<T> = 0> T ln(const T& x) { return std::log(x); }
template <class T, if_fixed<T> = 0> T ln(const T& x) { return math::ln(x); }
template <class T, if_std<T> = 0> T arccos(const T& x) { return std::acos(x); }
template <class T, if_fixed<T> = 0> T arccos(const T& x) { return math::arccos(x); }
template <class T, if_std<T> = 0> T arcsin(const T& x) { return std::asin(x); }
template <class T, if_fixed<T> = 0> T arcsin(const T& x) { return math::arcsin(x); }
template <class T, if_std<T> = 0> T atan2(const T& y, const T& x) { return std::atan2(y, x); }
template <class T, if_fixed<T> = 0> T atan2(const T& y, const T& x) { return math::atan2(y, x); }

template <class T>
struct Target {
    using value_type = T;

    std::size_t bits() const noexcept { return digits<T>(); }
    T           round(const BigFloat& v) const { return from_big<T>(v); }
    static bool same(const T& a, const T& b) { return identical(a, b); }
};

struct BigTarget {
    using value_type = BigFloat;

    BigFloatContext ctx;

    explicit BigTarget(const BigFloatContext& c) : ctx(c) {}

    std::size_t bits() const noexcept { return ctx.precision; }
    BigFloat    round(const BigFloat& v) const { return v.rounded(ctx); }
    static bool same(const BigFloat& a, const BigFloat& b) { return identical(a, b); }
};

namespace detail {

inline std::int64_t top_exponent(const BigFloat& v) {
    return v.exponent() + static_cast<std::int64_t>(v.significand().bit_length()) - 1;
}

template <class Tg>
bool round_checked(const Tg& target, const BigFloat& v, std::size_t w, typename Tg::value_type& out) {
    if (!v.is_finite() || v.is_zero()) {
        out = target.round(v);
        return true;
    }

    const std::int64_t top = top_exponent(v);
    const BigFloat delta(BigUInt::one(), false, top - static_cast<std::int64_t>(w) + 2);
    const BigFloatContext wide = context(w + 16);
    const typename Tg::value_type lo = target.round(BigFloat::sub(v, delta, wide));
    const typename Tg::value_type hi = target.round(BigFloat::add(v, delta, wide));
    if (!Tg::same(lo, hi)) return false;
    out = lo;
    return true;
}

constexpr int kMaxZivRounds = 6;

} // namespace detail

template <class Tg, class F>
typename Tg::value_type ziv(const Tg& target, F&& f) {
    std::size_t w = target.bits() + 32;
    BigFloat v;

    for (int round = 0; round < detail::kMaxZivRounds; ++round) {
        v = f(context(w));
        typename Tg::value_type out;
        if (detail::round_checked(target, v, w, out)) return out;
        w *= 2;
    }

    return target.round(v);
}

template <class Tg, std::size_t M, class F>
std::array<typename Tg::value_type, M> ziv_array(const Tg& target, F&& f) {
    std::size_t w = target.bits() + 48;
    std::array<BigFloat, M> v;
    std::array<typename Tg::value_type, M> prev;
    bool have_prev = false;

    for (int round = 0; round < detail::kMaxZivRounds; ++round) {
        v = f(context(w));
        std::array<typename Tg::value_type, M> out;
        bool ok = true;

        for (std::size_t i = 0; i < M; ++i) {
            if (!detail::round_checked(target, v[i], w, out[i])) { ok = false; break; }
        }

        if (ok && have_prev) {
            bool agree = true;
            for (std::size_t i = 0; i < M && agree; ++i) agree = Tg::same(out[i], prev[i]);
            if (agree) return out;
        }

        if (ok) {
            prev = out;
            have_prev = true;
        }

        w *= 2;
    }

    std::array<typename Tg::value_type, M> out;
    for (std::size_t i = 0; i < M; ++i) out[i] = target.round(v[i]);
    return out;
}

template <class Tg, class F>
std::vector<typename Tg::value_type> ziv_vector(const Tg& target, std::size_t count, F&& f) {
    std::size_t w = target.bits() + 48;
    std::vector<BigFloat> v;
    std::vector<typename Tg::value_type> prev;
    bool have_prev = false;

    for (int round = 0; round < detail::kMaxZivRounds; ++round) {
        v = f(context(w));
        std::vector<typename Tg::value_type> out(count);
        bool ok = v.size() == count;

        for (std::size_t i = 0; ok && i < count; ++i) {
            if (!detail::round_checked(target, v[i], w, out[i])) ok = false;
        }

        if (ok && have_prev) {
            bool agree = true;
            for (std::size_t i = 0; i < count && agree; ++i) agree = Tg::same(out[i], prev[i]);
            if (agree) return out;
        }

        if (ok) {
            prev = out;
            have_prev = true;
        }

        w *= 2;
    }

    std::vector<typename Tg::value_type> out(count);
    for (std::size_t i = 0; i < count && i < v.size(); ++i) out[i] = target.round(v[i]);
    return out;
}

template <class T, class F, if_real<T> = 0>
T evaluate(F&& f) { return ziv(Target<T>(), std::forward<F>(f)); }

inline BigFloat exact_mul(const BigFloat& a, const BigFloat& b) { return BigFloat::mul(a, b, exact()); }
inline BigFloat exact_add(const BigFloat& a, const BigFloat& b) { return BigFloat::add(a, b, exact()); }
inline BigFloat exact_sub(const BigFloat& a, const BigFloat& b) { return BigFloat::sub(a, b, exact()); }

template <class V>
BigFloat exact_dot(const V* a, const V* b, std::size_t n) {
    BigFloat s = BigFloat::zero();
    for (std::size_t i = 0; i < n; ++i) s = exact_add(s, exact_mul(BigFloat(a[i]), BigFloat(b[i])));
    return s;
}

inline BigFloat exact_dot(const BigFloat* a, const BigFloat* b, std::size_t n) {
    BigFloat s = BigFloat::zero();
    for (std::size_t i = 0; i < n; ++i) s = exact_add(s, exact_mul(a[i], b[i]));
    return s;
}

template <class V>
BigFloat exact_sum(const V* a, std::size_t n) {
    BigFloat s = BigFloat::zero();
    for (std::size_t i = 0; i < n; ++i) s = exact_add(s, BigFloat(a[i]));
    return s;
}

inline BigFloat exact_sum(const BigFloat* a, std::size_t n) {
    BigFloat s = BigFloat::zero();
    for (std::size_t i = 0; i < n; ++i) s = exact_add(s, a[i]);
    return s;
}

namespace detail {

template <class T>
struct fast_path : std::integral_constant<bool, std::is_same<T, float>::value || std::is_same<T, double>::value> {};

template <class T>
struct Dot2 {
    T    hi = T(0);
    T    lo = T(0);
    T    bound = T(0);
    bool zero_products = true;
};

template <class T>
constexpr T pow2(int k) noexcept {
    T v = T(1);
    for (; k > 0; --k) v *= T(2);
    for (; k < 0; ++k) v *= T(0.5);
    return v;
}

template <class T>
struct fp_consts {
    static constexpr int digits = std::numeric_limits<T>::digits;
    static constexpr T   u      = pow2<T>(-digits);
    static constexpr T   tiny   = std::numeric_limits<T>::min() * pow2<T>(digits + 2);
    static constexpr T   tiny2  = std::numeric_limits<T>::min() * pow2<T>(2 * digits + 4);
    static constexpr T   huge   = pow2<T>(std::numeric_limits<T>::max_exponent - digits);
    static constexpr T   split  = pow2<T>((digits + 1) / 2) + T(1);
};

template <class T>
struct fp_bits;

template <>
struct fp_bits<double> {
    using type = std::uint64_t;
    static constexpr int mantissa = 52;
    static constexpr type exp_mask = 0x7ffULL;
};

template <>
struct fp_bits<float> {
    using type = std::uint32_t;
    static constexpr int mantissa = 23;
    static constexpr type exp_mask = 0xffU;
};

template <class T>
T min_spacing(const T& r) noexcept {
    using B = fp_bits<T>;
    typename B::type bits;
    const T a = std::fabs(r);
    std::memcpy(&bits, &a, sizeof(T));
    const typename B::type e = (bits >> B::mantissa) & B::exp_mask;
    const bool pow_two = (bits & ((typename B::type(1) << B::mantissa) - 1)) == 0;
    typename B::type ue = e - static_cast<typename B::type>(B::mantissa) - (pow_two ? 1 : 0);
    typename B::type out_bits = ue << B::mantissa;
    T out;
    std::memcpy(&out, &out_bits, sizeof(T));
    return out;
}

template <class T>
T product_error(const T& a, const T& b, const T& p) noexcept {
#if defined(__FMA__) || defined(__FP_FAST_FMA) || defined(FP_FAST_FMA)
    return std::fma(a, b, -p);
#else
    const T ca = fp_consts<T>::split * a;
    const T ah = ca - (ca - a);
    const T al = a - ah;
    const T cb = fp_consts<T>::split * b;
    const T bh = cb - (cb - b);
    const T bl = b - bh;
    return ((ah * bh - p) + ah * bl + al * bh) + al * bl;
#endif
}

template <class T, typename std::enable_if<fast_path<T>::value, int>::type = 0>
bool dot2(const T* a, const T* b, std::size_t n, Dot2<T>& out) {
    const T tiny = fp_consts<T>::tiny;
    const T huge = fp_consts<T>::huge;
    const T u = fp_consts<T>::u;
    T stack_terms[64];
    std::vector<T> heap_terms;
    T* t = stack_terms;

    if (2 * n > 64) {
        heap_terms.resize(2 * n);
        t = heap_terms.data();
    }

    std::size_t m = 0;
    bool zero = true;

    for (std::size_t i = 0; i < n; ++i) {
        const T h = a[i] * b[i];
        if (!std::isfinite(h)) return false;

        if (h == T(0)) {
            if (a[i] != T(0) && b[i] != T(0)) return false;
            continue;
        }

        if (std::fabs(h) < tiny || std::fabs(a[i]) > huge || std::fabs(b[i]) > huge) return false;
        zero = false;
        t[m++] = h;
        const T q = product_error(a[i], b[i], h);
        if (q != T(0)) t[m++] = q;
    }

    out.zero_products = zero;

    if (m == 0) {
        out.hi = T(0);
        out.lo = T(0);
        out.bound = T(0);
        return true;
    }

    const T target = std::numeric_limits<T>::epsilon() * T(1.0 / 1073741824.0);

    for (int pass = 0; pass < 4; ++pass) {
        if (pass > 0) {
            std::size_t k = 0;
            for (std::size_t i = 0; i + 1 < m; ++i) if (t[i] != T(0)) t[k++] = t[i];
            t[k] = t[m - 1];
            m = k + 1;
        }

        for (std::size_t i = 1; i < m; ++i) {
            const T x = t[i - 1];
            const T y = t[i];
            const T sum = x + y;
            const T bb = sum - x;
            t[i] = sum;
            t[i - 1] = (x - (sum - bb)) + (y - bb);
        }

        if (!std::isfinite(t[m - 1])) return false;
        T lo = T(0);
        T sabs = T(0);

        for (std::size_t i = 0; i + 1 < m; ++i) {
            lo = lo + t[i];
            sabs = sabs + std::fabs(t[i]);
        }

        const T hi = t[m - 1];
        if (!std::isfinite(sabs)) return false;
        const T mm = static_cast<T>(2 * m + 4);
        const T bound = sabs == T(0) ? T(0) : sabs * (mm * u) * (T(1) + mm * u) + mm * std::numeric_limits<T>::denorm_min();

        if (pass == 3 || sabs == T(0) || bound <= target * std::fabs(hi)) {
            const T r = hi + lo;
            if (!std::isfinite(r)) return false;
            const T bb = r - hi;
            out.hi = r;
            out.lo = (hi - (r - bb)) + (lo - bb);
            out.bound = bound;
            return true;
        }
    }

    return false;
}

template <class T, typename std::enable_if<fast_path<T>::value, int>::type = 0>
bool fast_dot(const T* a, const T* b, std::size_t n, T& out) {
    Dot2<T> d;
    if (!dot2(a, b, n, d)) return false;

    if (d.zero_products) {
        T s = a[0] * b[0];
        for (std::size_t i = 1; i < n; ++i) s = s + a[i] * b[i];
        out = s;
        return true;
    }

    if (d.lo == T(0) && d.bound == T(0)) {
        out = d.hi;
        return true;
    }

    if (std::fabs(d.hi) < fp_consts<T>::tiny) return false;
    if (!(std::fabs(d.lo) + d.bound < min_spacing(d.hi) * T(0.5))) return false;
    out = d.hi;
    return true;
}

template <class T, typename std::enable_if<!fast_path<T>::value, int>::type = 0>
bool fast_dot(const T*, const T*, std::size_t, T&) { return false; }

template <class T, typename std::enable_if<fast_path<T>::value, int>::type = 0>
bool fast_norm(const T* a, std::size_t n, T& out) {
    Dot2<T> d;
    if (!dot2(a, a, n, d) || d.zero_products) return false;

    if (!(d.hi > fp_consts<T>::tiny2)) return false;

    const T u = fp_consts<T>::u;
    T y = std::sqrt(d.hi);
    T dd = T(0);
    T d2 = T(0);

    for (int step = 0; step < 2; ++step) {
        const T ph = y * y;
        const T pl = product_error(y, y, ph);
        const T d1 = d.hi - ph;
        d2 = d.lo - pl;
        dd = d1 + d2;
        if (step == 1) break;
        const T y1 = y + dd / (T(2) * y);
        if (y1 == y) break;
        y = y1;
    }

    const T err = std::fabs(dd) + T(4) * u * (std::fabs(d2) + std::fabs(dd)) + d.bound;
    const T h = min_spacing(y);
    if (!(err < y * h * (T(1) - T(1) / T(1048576)))) return false;
    out = y;
    return true;
}

template <class T, typename std::enable_if<!fast_path<T>::value, int>::type = 0>
bool fast_norm(const T*, std::size_t, T&) { return false; }

template <class T>
struct DDSqrt {
    T y = T(0);
    T yl = T(0);
    T err = T(0);
};

template <class T, typename std::enable_if<fast_path<T>::value, int>::type = 0>
bool dd_sqrt_of_dot(const T* a, std::size_t n, DDSqrt<T>& out) {
    Dot2<T> d;
    if (!dot2(a, a, n, d) || d.zero_products) return false;
    if (!(d.hi > fp_consts<T>::tiny2)) return false;

    const T u = fp_consts<T>::u;
    T y = std::sqrt(d.hi);
    T dd = T(0);
    T d2 = T(0);

    for (int step = 0; step < 2; ++step) {
        const T ph = y * y;
        const T pl = product_error(y, y, ph);
        const T d1 = d.hi - ph;
        d2 = d.lo - pl;
        dd = d1 + d2;
        if (step == 1) break;
        const T y1 = y + dd / (T(2) * y);
        if (y1 == y) break;
        y = y1;
    }

    const T errd = T(4) * u * (std::fabs(d2) + std::fabs(dd)) + d.bound;
    out.y = y;
    out.yl = dd / (T(2) * y);
    out.err = T(2) * (u * std::fabs(out.yl) + errd / (T(2) * y) + y * u * u);
    return true;
}

template <class T>
bool checked_quotient(const T& x, const DDSqrt<T>& n, T& out) {
    if (x == T(0)) {
        out = x;
        return true;
    }

    const T u = fp_consts<T>::u;
    T c = x / n.y;

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!(std::fabs(c) > fp_consts<T>::tiny) || !std::isfinite(c)) return false;
        const T ph = c * n.y;
        const T pl = product_error(c, n.y, ph);
        const T r1 = x - ph;
        const T r2 = r1 - pl;
        const T cyl = c * n.yl;
        const T r = r2 - cyl;
        const T err = u * (std::fabs(r2) + T(2) * std::fabs(cyl) + std::fabs(r)) + std::fabs(c) * n.err;
        const T h = min_spacing(c);

        if (std::fabs(r) + err < T(0.5) * h * n.y * (T(1) - T(1) / T(1048576))) {
            out = c;
            return true;
        }

        const T c2 = c + r / n.y;
        if (c2 == c) return false;
        c = c2;
    }

    return false;
}

template <class T, typename std::enable_if<fast_path<T>::value, int>::type = 0>
bool fast_unit(const T* a, std::size_t n, T* out) {
    DDSqrt<T> s;
    if (!dd_sqrt_of_dot(a, n, s)) return false;
    for (std::size_t i = 0; i < n; ++i) if (!checked_quotient(a[i], s, out[i])) return false;
    return true;
}

template <class T, typename std::enable_if<!fast_path<T>::value, int>::type = 0>
bool fast_unit(const T*, std::size_t, T*) { return false; }

template <class T, typename std::enable_if<fast_path<T>::value, int>::type = 0>
bool fast_ratio(const T* na, const T* nb, std::size_t nn, const T* da, const T* db, std::size_t nd, T& out) {
    Dot2<T> num;
    Dot2<T> den;
    if (!dot2(na, nb, nn, num) || !dot2(da, db, nd, den)) return false;
    if (den.zero_products) return false;

    if (num.zero_products || (num.hi == T(0) && num.lo == T(0) && num.bound == T(0))) {
        if (!(std::fabs(den.hi) > fp_consts<T>::tiny)) return false;
        out = std::signbit(den.hi) ? -T(0) : T(0);
        return true;
    }

    const T u = fp_consts<T>::u;
    const T dh = den.hi;
    if (!(std::fabs(dh) > fp_consts<T>::tiny) || !(std::fabs(num.hi) > fp_consts<T>::tiny)) return false;
    const T dmin = std::fabs(dh) - std::fabs(den.lo) - den.bound;
    if (!(dmin > T(0))) return false;

    T c = num.hi / dh;

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!(std::fabs(c) > fp_consts<T>::tiny) || !std::isfinite(c)) return false;
        const T ph = c * dh;
        const T pl = product_error(c, dh, ph);
        const T r1 = num.hi - ph;
        const T r2 = r1 - pl;
        const T r3 = r2 + num.lo;
        const T cdl = c * den.lo;
        const T r = r3 - cdl;
        const T err = T(2) * u * (std::fabs(r2) + std::fabs(r3) + T(2) * std::fabs(cdl) + std::fabs(r)) + num.bound + std::fabs(c) * den.bound;

        if (std::fabs(r) + err < T(0.5) * min_spacing(c) * dmin * (T(1) - T(1) / T(1048576))) {
            out = c;
            return true;
        }

        const T c2 = c + r / dh;
        if (c2 == c) return false;
        c = c2;
    }

    return false;
}

template <class T, typename std::enable_if<!fast_path<T>::value, int>::type = 0>
bool fast_ratio(const T*, const T*, std::size_t, const T*, const T*, std::size_t, T&) { return false; }

template <class T>
bool all_finite(const T* a, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) if (!is_finite(a[i])) return false;
    return true;
}

template <class T>
T naive_dot(const T* a, const T* b, std::size_t n) {
    T s = T(0);
    for (std::size_t i = 0; i < n; ++i) s = s + a[i] * b[i];
    return s;
}

} // namespace detail

template <class T, if_real<T> = 0>
T dot(const T* a, const T* b, std::size_t n) {
    if (n == 0) return T(0);
    if (n == 1) return a[0] * b[0];
    T out;
    if (detail::fast_dot(a, b, n, out)) return out;
    if (!detail::all_finite(a, n) || !detail::all_finite(b, n)) return detail::naive_dot(a, b, n);
    return from_big<T>(exact_dot(a, b, n));
}

template <class T, if_real<T> = 0>
T dot(std::initializer_list<T> a, std::initializer_list<T> b) {
    return dot(a.begin(), b.begin(), a.size() < b.size() ? a.size() : b.size());
}

template <class T, if_real<T> = 0>
T sum(const T* a, std::size_t n) {
    if (n == 0) return T(0);
    if (n == 1) return a[0];
    if (n == 2) return a[0] + a[1];

    if (!detail::all_finite(a, n)) {
        T s = T(0);
        for (std::size_t i = 0; i < n; ++i) s = s + a[i];
        return s;
    }

    return from_big<T>(exact_sum(a, n));
}

template <class T, if_real<T> = 0>
T sum_of_squares(const T* a, std::size_t n) { return dot(a, a, n); }

template <class T, if_real<T> = 0>
T diff_of_products(const T& a, const T& b, const T& c, const T& d) {
    const T x[2] = { a, c };
    const T y[2] = { b, -d };
    return dot(x, y, 2);
}

template <class T, if_real<T> = 0>
T sum_of_products(const T& a, const T& b, const T& c, const T& d) {
    const T x[2] = { a, c };
    const T y[2] = { b, d };
    return dot(x, y, 2);
}

template <class T, if_real<T> = 0>
T norm(const T* a, std::size_t n) {
    if (n == 0) return T(0);
    if (n == 1) return abs(a[0]);

    for (std::size_t i = 0; i < n; ++i) if (is_inf(a[i])) return infinity<T>();
    if (!detail::all_finite(a, n)) return nan<T>();
    T fast;
    if (detail::fast_norm(a, n, fast)) return fast;
    const BigFloat s = exact_dot(a, a, n);
    if (s.is_zero()) return T(0);
    return evaluate<T>([&s](const BigFloatContext& c) { return math::sqrt(s, c); });
}

template <class T, if_real<T> = 0>
T hypot(const T& a, const T& b) {
    const T v[2] = { a, b };
    return norm(v, 2);
}

template <class T, if_real<T> = 0>
T pi() {
    static const T value = evaluate<T>([](const BigFloatContext& c) { return multiprecision::constants::pi(c); });
    return value;
}

template <class T, if_real<T> = 0>
T half_pi() {
    static const T value = evaluate<T>([](const BigFloatContext& c) { return multiprecision::constants::half_pi(c); });
    return value;
}

template <class T, if_real<T> = 0>
T degrees_to_radians(const T& degrees) {
    const BigFloat d(degrees);
    if (!d.is_finite()) return degrees;
    return evaluate<T>([&d](const BigFloatContext& c) {
        const BigFloatContext w = context(c.precision + 8);
        return BigFloat::div(BigFloat::mul(d, multiprecision::constants::pi(w), w), BigFloat(180), c);
    });
}

template <class T, if_real<T> = 0>
T radians_to_degrees(const T& radians) {
    const BigFloat r(radians);
    if (!r.is_finite()) return radians;
    return evaluate<T>([&r](const BigFloatContext& c) {
        const BigFloatContext w = context(c.precision + 8);
        return BigFloat::div(BigFloat::mul(r, BigFloat(180), w), multiprecision::constants::pi(w), c);
    });
}

inline BigFloat degrees_to_radians(const BigFloat& degrees, const BigFloatContext& ctx) {
    if (!degrees.is_finite()) return degrees;
    return ziv(BigTarget(ctx), [&degrees](const BigFloatContext& c) {
        return BigFloat::div(BigFloat::mul(degrees, multiprecision::constants::pi(c), c), BigFloat(180), c);
    });
}

inline BigFloat radians_to_degrees(const BigFloat& radians, const BigFloatContext& ctx) {
    if (!radians.is_finite()) return radians;
    return ziv(BigTarget(ctx), [&radians](const BigFloatContext& c) {
        return BigFloat::div(BigFloat::mul(radians, BigFloat(180), c), multiprecision::constants::pi(c), c);
    });
}

inline BigFloat dot(const BigFloat* a, const BigFloat* b, std::size_t n, const BigFloatContext& ctx) {
    return exact_dot(a, b, n).rounded(ctx);
}

inline BigFloat sum(const BigFloat* a, std::size_t n, const BigFloatContext& ctx) {
    return exact_sum(a, n).rounded(ctx);
}

inline BigFloat diff_of_products(const BigFloat& a, const BigFloat& b, const BigFloat& c, const BigFloat& d, const BigFloatContext& ctx) {
    return exact_sub(exact_mul(a, b), exact_mul(c, d)).rounded(ctx);
}

inline BigFloat norm(const BigFloat* a, std::size_t n, const BigFloatContext& ctx) {
    for (std::size_t i = 0; i < n; ++i) if (a[i].is_infinite()) return BigFloat::infinity();
    const BigFloat s = exact_dot(a, a, n);
    if (s.is_zero()) return BigFloat::zero();
    return math::sqrt(s, ctx);
}

template <class T, if_std<T> = 0>
std::string to_string(const T& x) {
    std::ostringstream os;
    os.precision(std::numeric_limits<T>::max_digits10);
    os << x;
    return os.str();
}

template <class T, if_fixed<T> = 0>
std::string to_string(const T& x) { return x.to_string(); }

inline std::string to_string(const BigFloat& x) { return x.to_string(); }

} // namespace scalar
} // namespace fizmo

#endif // FIZMO_SCALAR_HPP
