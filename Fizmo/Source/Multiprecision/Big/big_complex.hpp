#ifndef FIZMO_MULTIPRECISION_BIG_COMPLEX_HPP
#define FIZMO_MULTIPRECISION_BIG_COMPLEX_HPP

#include <cmath>
#include <complex>

#include "BigFloat Math/big_float_consts.hpp"
#include "BigFloat Math/queries.hpp"
#include "BigFloat Math/exp.hpp"
#include "BigFloat Math/num_theory.hpp"
#include "BigFloat Math/sqrt_cbrt.hpp"
#include "BigFloat Math/inv_htrig.hpp"
#include "BigFloat Math/pow_nth_root.hpp"
#include "BigFloat Math/erf.hpp"
#include "BigFloat Math/stieltjes.hpp"
#include "BigFloat Math/algebraic_consts.hpp"
#include "BigFloat Math/big_int_sequences.hpp"
#include "BigFloat Math/gamma.hpp"
#include "BigFloat Math/polygamma.hpp"
#include "BigFloat Math/polynomials.hpp"
#include "BigFloat Math/harmonic.hpp"
#include "BigFloat Math/gudermannian.hpp"
#include "BigFloat Math/riemann_zeta.hpp"
#include "BigFloat Math/generalized_gaussian.hpp"
#include "BigFloat Math/polylog.hpp"
#include "BigFloat Math/legendre.hpp"
#include "BigFloat Math/dirichlet.hpp"
#include "BigFloat Math/hurwitz_lerch.hpp"
#include "BigFloat Math/airy.hpp"
#include "BigFloat Math/dilog_family.hpp"
#include "BigFloat Math/elliptic.hpp"
#include "BigFloat Math/jacobi_elliptic.hpp"
#include "BigFloat Math/lambert_w.hpp"
#include "BigFloat Math/special_integrals.hpp"
#include "BigFloat Math/combinatorics.hpp"

namespace fizmo {
namespace multiprecision {
struct BigComplexContext {
    std::size_t  real_precision;
    std::size_t  imaginary_precision;
    RoundingMode real_rounding;
    RoundingMode imaginary_rounding;

    constexpr BigComplexContext() noexcept
        : real_precision(BigFloatContext::default_prec), imaginary_precision(BigFloatContext::default_prec),
          real_rounding(RoundingMode::nearest_even),     imaginary_rounding(RoundingMode::nearest_even) {}

    explicit constexpr BigComplexContext(const BigFloatContext& c) noexcept
        : real_precision(c.precision),     imaginary_precision(c.precision),
          real_rounding(c.rounding_mode),  imaginary_rounding(c.rounding_mode) {}

    constexpr BigComplexContext(const BigFloatContext& re, const BigFloatContext& im) noexcept
        : real_precision(re.precision),    imaginary_precision(im.precision),
          real_rounding(re.rounding_mode), imaginary_rounding(im.rounding_mode) {}

    explicit constexpr BigComplexContext(std::size_t p, RoundingMode r = RoundingMode::nearest_even) noexcept
        : real_precision(BigFloatContext::clamp_precision(p)), imaginary_precision(BigFloatContext::clamp_precision(p)),
          real_rounding(r),                                    imaginary_rounding(r) {}

    constexpr BigComplexContext(std::size_t p_re, std::size_t p_im,
                                RoundingMode r_re = RoundingMode::nearest_even,
                                RoundingMode r_im = RoundingMode::nearest_even) noexcept
        : real_precision(BigFloatContext::clamp_precision(p_re)), imaginary_precision(BigFloatContext::clamp_precision(p_im)),
          real_rounding(r_re),                                    imaginary_rounding(r_im) {}

    constexpr BigFloatContext real() const noexcept { return BigFloatContext(real_precision, real_rounding); }
    constexpr BigFloatContext imaginary() const noexcept { return BigFloatContext(imaginary_precision, imaginary_rounding); }

    constexpr std::size_t     precision() const noexcept { return real_precision > imaginary_precision ? real_precision : imaginary_precision; }
    constexpr BigFloatContext scalar()    const noexcept { return BigFloatContext(precision(), real_rounding); }
    constexpr bool            is_uniform() const noexcept { return real_precision == imaginary_precision && real_rounding == imaginary_rounding; }

    std::size_t dps() const noexcept { return BigFloatContext::dps_from_precision(precision()); }

    static constexpr BigComplexContext with_dps(std::size_t dps, RoundingMode r = RoundingMode::nearest_even) noexcept {
        return BigComplexContext(BigFloatContext::precision_from_dps(dps), r);
    }

    BigComplexContext extended(std::size_t guard_bits) const noexcept {
        return BigComplexContext(real_precision + guard_bits, imaginary_precision + guard_bits, real_rounding, imaginary_rounding);
    }

    BigComplexContext with_rounding(RoundingMode r) const noexcept {
        return BigComplexContext(real_precision, imaginary_precision, r, r);
    }

    static BigComplexContext current() noexcept;
    static void set_current(const BigComplexContext& c) noexcept;
    static void follow_float_context() noexcept;
    static bool follows_float_context() noexcept;

    friend constexpr bool operator==(const BigComplexContext& a, const BigComplexContext& b) noexcept {
        return a.real_precision == b.real_precision && a.imaginary_precision == b.imaginary_precision &&
               a.real_rounding  == b.real_rounding  && a.imaginary_rounding  == b.imaginary_rounding;
    }

    friend constexpr bool operator!=(const BigComplexContext& a, const BigComplexContext& b) noexcept { return !(a == b); }
};

namespace bcdetail {
struct context_state {
    bool              overridden;
    BigComplexContext ctx;
};

inline context_state& context_tls() noexcept {
    static thread_local context_state s = { false, BigComplexContext() };
    return s;
}

} // namespace bcdetail

inline BigComplexContext BigComplexContext::current() noexcept {
    const bcdetail::context_state& s = bcdetail::context_tls();
    return s.overridden ? s.ctx : BigComplexContext(BigFloatContext::current());
}

inline void BigComplexContext::set_current(const BigComplexContext& c) noexcept {
    bcdetail::context_state& s = bcdetail::context_tls();
    s.overridden = true;
    s.ctx        = c;
}

inline void BigComplexContext::follow_float_context() noexcept { bcdetail::context_tls().overridden = false; }
inline bool BigComplexContext::follows_float_context() noexcept { return !bcdetail::context_tls().overridden; }

class ScopedComplexContext {
private:
    bcdetail::context_state m_saved;

public:
    explicit ScopedComplexContext(const BigComplexContext& c) noexcept : m_saved(bcdetail::context_tls()) { BigComplexContext::set_current(c); }

    explicit ScopedComplexContext(std::size_t prec) noexcept
        : ScopedComplexContext(BigComplexContext(prec, BigComplexContext::current().real_rounding)) {}

    ScopedComplexContext(std::size_t prec, RoundingMode rnd) noexcept : ScopedComplexContext(BigComplexContext(prec, rnd)) {}

    ScopedComplexContext(std::size_t p_re, std::size_t p_im) noexcept
        : ScopedComplexContext(BigComplexContext(p_re, p_im, BigComplexContext::current().real_rounding, BigComplexContext::current().imaginary_rounding)) {}

    ~ScopedComplexContext() noexcept { bcdetail::context_tls() = m_saved; }

    ScopedComplexContext(const ScopedComplexContext&)            = delete;
    ScopedComplexContext& operator=(const ScopedComplexContext&) = delete;
};

namespace bcdetail {
using BF  = BigFloat;
using BFC = BigFloatContext;

constexpr std::size_t k_exact = std::size_t(1) << 60;

struct dyadic {
    BigUInt      mag;
    std::int64_t exp;
    bool         neg;

    dyadic() : mag(BigUInt::zero()), exp(0), neg(false) {}
    dyadic(BigUInt m, std::int64_t e, bool n) : mag(std::move(m)), exp(e), neg(n) {}
};

inline bool         dy_zero(const dyadic& d) noexcept { return d.mag.is_zero(); }
inline bool         dy_bad (const dyadic& d) noexcept { return d.mag.is_undefined(); }
inline std::int64_t dy_len (const dyadic& d) noexcept { return static_cast<std::int64_t>(d.mag.bit_length()); }
inline std::int64_t dy_top (const dyadic& d) noexcept { return d.exp + dy_len(d); }

inline dyadic dy_bad_value() { return dyadic(BigUInt::undefined(), 0, false); }

inline dyadic dy_of(const BF& x) {
    if (x.is_zero()) return dyadic(BigUInt::zero(), 0, x.signbit());
    return dyadic(x.significand(), x.exponent(), x.signbit());
}

inline dyadic dy_neg(dyadic d) { d.neg = !d.neg; return d; }

inline dyadic dy_mul(const BF& x, const BF& y) {
    const bool neg = (x.signbit() != y.signbit());
    if (x.is_zero() || y.is_zero()) return dyadic(BigUInt::zero(), 0, neg);
    return dyadic(x.significand() * y.significand(), x.exponent() + y.exponent(), neg);
}

inline dyadic dy_square(const BF& x) {
    if (x.is_zero()) return dyadic();
    return dyadic(x.significand() * x.significand(), 2 * x.exponent(), false);
}

inline dyadic dy_twice(dyadic d) { if (!dy_zero(d)) ++d.exp; return d; }

inline BF dy_unit(const dyadic& d) { return BF(d.mag, d.neg, -dy_len(d)); }

inline BF dy_round(const dyadic& d, const BFC& ctx) {
    if (dy_bad(d))  return BF::undefined();
    if (dy_zero(d)) return BF::zero(d.neg);
    return dy_unit(d).rounded(ctx).scaled2(dy_top(d));
}

inline BF dy_exact(const dyadic& d) {
    if (dy_bad(d))  return BF::undefined();
    if (dy_zero(d)) return BF::zero(d.neg);
    return BF(d.mag, d.neg, d.exp);
}

inline dyadic dy_add_exact(const dyadic& a, const dyadic& b) {
    const std::int64_t lo = (a.exp < b.exp) ? a.exp : b.exp;
    BigUInt A = a.mag;
    BigUInt B = b.mag;
    A.shift_left_mutable(static_cast<std::size_t>(a.exp - lo));
    B.shift_left_mutable(static_cast<std::size_t>(b.exp - lo));
    if (A.is_undefined() || B.is_undefined()) return dy_bad_value();
    if (a.neg == b.neg) { A.add_mutable(B); return dyadic(std::move(A), lo, a.neg); }
    const int c = A.compare(B);
    if (c == 0) return dyadic(BigUInt::zero(), 0, false);
    if (c > 0) { A.sub_mutable(B); return dyadic(std::move(A), lo, a.neg); }
    B.sub_mutable(A);
    return dyadic(std::move(B), lo, b.neg);
}

inline dyadic dy_sum(const dyadic& a, const dyadic& b, std::size_t K, RoundingMode mode, bool& exact) {
    exact = true;
    if (dy_bad(a) || dy_bad(b)) return dy_bad_value();
    const bool az = dy_zero(a), bz = dy_zero(b);
    const bool zero_neg_on_cancel = (mode == RoundingMode::toward_neg_inf);
    if (az && bz) return dyadic(BigUInt::zero(), 0, (a.neg == b.neg) ? a.neg : zero_neg_on_cancel);
    if (az) return b;
    if (bz) return a;
    const std::int64_t ta = dy_top(a), tb = dy_top(b);
    const bool          a_big = (ta >= tb);
    const dyadic&       L     = a_big ? a : b;
    const dyadic&       S     = a_big ? b : a;
    const std::int64_t  tL    = a_big ? ta : tb;
    const std::int64_t  tS    = a_big ? tb : ta;
    const bool          opp   = (a.neg != b.neg);
    const std::int64_t  lo    = (L.exp < S.exp) ? L.exp : S.exp;
    const std::uint64_t width  = static_cast<std::uint64_t>(tL - lo);
    const std::uint64_t budget = static_cast<std::uint64_t>(K) + L.mag.bit_length() + S.mag.bit_length() + 64;

    if ((opp && tL - tS < 2) || width <= budget) {
        dyadic r = dy_add_exact(a, b);
        if (dy_zero(r)) r.neg = zero_neg_on_cancel;
        return r;
    }

    exact = false;
    const std::int64_t tK = tL - static_cast<std::int64_t>(K);
    const std::int64_t F  = (tK < L.exp) ? tK : L.exp;
    BigUInt m = L.mag;
    m.shift_left_mutable(static_cast<std::size_t>(L.exp - F) + 1);
    if (opp) m.sub_small_mutable(1); else m.add_small_mutable(1);
    return dyadic(std::move(m), F - 1, L.neg);
}

inline dyadic dy_truncate(const dyadic& d, std::size_t K, bool& exact) {
    if (dy_bad(d) || dy_zero(d)) return d;
    const std::size_t len = d.mag.bit_length();
    if (len <= K + 1) return d;
    const std::size_t drop = len - K;
    const long long   ctz  = d.mag.count_trailing_zeros();
    BigUInt m = d.mag;
    m.shift_right_mutable(drop);
    if (ctz >= 0 && static_cast<std::size_t>(ctz) >= drop) return dyadic(std::move(m), d.exp + static_cast<std::int64_t>(drop), d.neg);
    exact = false;
    m.shift_left_mutable(1);
    m.add_small_mutable(1);
    return dyadic(std::move(m), d.exp + static_cast<std::int64_t>(drop) - 1, d.neg);
}

inline BF dy_divide(const dyadic& n, const dyadic& d, const BFC& ctx) {
    if (dy_bad(n) || dy_bad(d) || dy_zero(d)) return BF::undefined();
    if (dy_zero(n)) return BF::zero(n.neg != d.neg);
    return BF::div(dy_unit(n), dy_unit(d), ctx).scaled2(dy_top(n) - dy_top(d));
}

template <typename Num, typename Den>
inline BF dy_quotient(const Num& num, const Den& den, const BFC& ctx) {
    std::size_t guard = 32;

    for (;;) {
        const std::size_t w = BFC::clamp_precision(ctx.precision + guard);
        const std::size_t K = w + 4;
        bool nx = true, dx = true;
        const dyadic N = num(K, nx);
        const dyadic D = den(K, dx);
        if (dy_bad(N) || dy_bad(D) || dy_zero(D)) return BF::undefined();
        if (dy_zero(N)) return BF::zero(N.neg != D.neg);
        const std::size_t small = 2 * K + 64;
        if (nx && dx && N.mag.bit_length() <= small && D.mag.bit_length() <= small) return dy_divide(N, D, ctx);

        bool tnx = nx, tdx = dx;
        const dyadic n = dy_truncate(N, K, tnx);
        const dyadic d = dy_truncate(D, K, tdx);
        if (tnx && tdx) return dy_divide(n, d, ctx);
        const std::int64_t shift = dy_top(n) - dy_top(d);
        const BF q = BF::div(dy_unit(n), dy_unit(d), BFC(w, RoundingMode::nearest_even));

        if (q.is_finite() && !q.is_zero()) {
            const BF  delta(BigUInt::one(), false, q.get_exp_base2() + 3 - static_cast<std::int64_t>(w));
            const BFC xc(BFC::max_prec, ctx.rounding_mode);
            const BF  lo = BF::sub(q, delta, xc).rounded(ctx);
            const BF  hi = BF::add(q, delta, xc).rounded(ctx);
            if (BF::compare(lo, hi) == BF::ordering::equal) return lo.scaled2(shift);
        }

        if (nx && dx) return dy_divide(N, D, ctx);
        if (guard >= (std::size_t(1) << 16) || ctx.precision + guard >= BFC::max_prec / 4) return q.rounded(ctx).scaled2(shift);
        guard *= 2;
    }
}

inline int dy_cmp_abs(const dyadic& x, const dyadic& y) {
    const bool xz = dy_zero(x), yz = dy_zero(y);
    if (xz || yz) return xz ? (yz ? 0 : -1) : 1;
    const std::int64_t tx = dy_top(x), ty = dy_top(y);
    if (tx != ty) return (tx < ty) ? -1 : 1;
    if (x.exp == y.exp) return x.mag.compare(y.mag);

    if (x.exp > y.exp) {
        BigUInt t = x.mag;
        t.shift_left_mutable(static_cast<std::size_t>(x.exp - y.exp));
        return t.compare(y.mag);
    }

    BigUInt t = y.mag;
    t.shift_left_mutable(static_cast<std::size_t>(y.exp - x.exp));
    return x.mag.compare(t);
}

inline bool dy_order(const dyadic& a, bool a_exact, const dyadic& b, bool b_exact, int& out) {
    if (a_exact && b_exact) { out = dy_cmp_abs(a, b); return true; }
    dyadic a_lo = a, a_hi = a, b_lo = b, b_hi = b;
    if (!a_exact) { a_lo.mag.sub_small_mutable(1); a_hi.mag.add_small_mutable(1); }
    if (!b_exact) { b_lo.mag.sub_small_mutable(1); b_hi.mag.add_small_mutable(1); }
    if (dy_cmp_abs(a_hi, b_lo) <= 0) { out = -1; return true; }
    if (dy_cmp_abs(a_lo, b_hi) >= 0) { out =  1; return true; }
    return false;
}

inline std::size_t bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline std::uint64_t abs_u64(std::int64_t v) noexcept {
    return (v < 0) ? (~static_cast<std::uint64_t>(v) + 1u) : static_cast<std::uint64_t>(v);
}

inline int dy_sign(const dyadic& d) noexcept { return dy_zero(d) ? 0 : (d.neg ? -1 : 1); }

inline int dy_sign_of_difference(const dyadic& p, const dyadic& q) {
    const int sp = dy_sign(p), sq = dy_sign(q);
    if (sp != sq) return (sp > sq) ? 1 : -1;
    if (sp == 0) return 0;
    const int c = dy_cmp_abs(p, q);
    return (sp > 0) ? c : -c;
}

inline int argument_class(const BF& x, const BF& y) noexcept {
    if (y.is_zero()) return x.signbit() ? (y.signbit() ? 0 : 4) : 2;
    return y.signbit() ? 1 : 3;
}

template <typename T>
inline void swap_by_move(T& a, T& b) {
    T t(std::move(a));
    a = std::move(b);
    b = std::move(t);
}

inline bool symmetric(RoundingMode r) noexcept {
    return r == RoundingMode::nearest_even || r == RoundingMode::nearest_away || r == RoundingMode::toward_zero;
}

inline BF to_infinity(const BF& s) { return s.is_zero() ? s : BF::infinity(s.signbit()); }
inline BF to_zero(const BF& s)     { return BF::zero(s.signbit()); }

inline BigFloat::ordering to_ordering(int c) noexcept {
    return (c < 0) ? BigFloat::ordering::less : ((c > 0) ? BigFloat::ordering::greater : BigFloat::ordering::equal);
}

} // namespace bcdetail

class BigComplex;

namespace bcdetail {
template <typename T>
struct is_real_operand : std::integral_constant<bool,
    !std::is_same<typename std::decay<T>::type, BigComplex>::value &&
    std::is_convertible<const T&, BigFloat>::value> {};

inline const BigFloat& as_float(const BigFloat& x) noexcept { return x; }

template <typename R, typename std::enable_if<!std::is_same<R, BigFloat>::value, int>::type = 0>
inline BigFloat as_float(const R& x) { return BigFloat(x); }

} // namespace bcdetail

class BigComplex {
public:
    enum class fpclass : std::uint8_t { finite = 0, infinite, undefined };
    using ordering = BigFloat::ordering;

private:
    struct slot {
        BigFloat     value;
        std::size_t  prec;
        RoundingMode mode;
        bool         valid;

        slot() : value(), prec(0), mode(RoundingMode::nearest_even), valid(false) {}

        bool hit(const BigFloatContext& c) const noexcept { return valid && prec == c.precision && mode == c.rounding_mode; }

        const BigFloat& put(BigFloat v, const BigFloatContext& c) {
            value = std::move(v);
            prec  = c.precision;
            mode  = c.rounding_mode;
            valid = true;
            return value;
        }
    };

    struct pair_slot {
        BigFloat          re, im;
        BigComplexContext ctx;
        bool              valid;

        pair_slot() : re(), im(), ctx(), valid(false) {}
    };

    struct cache_t {
        bcdetail::dyadic real_sq, imaginary_sq;
        bcdetail::dyadic norm;
        std::size_t      norm_k;
        bool             have_sq, have_norm, norm_exact;
        slot             norm_r, abs_r, arg_r, arg_w, branch_r;
        std::int64_t     branch_k;
        pair_slot        recip;

        cache_t() : real_sq(), imaginary_sq(), norm(), norm_k(0), have_sq(false), have_norm(false), norm_exact(false), branch_k(0) {}
    };

    BigFloat m_real;
    BigFloat m_imaginary;
    mutable std::unique_ptr<cache_t> m_cache;

    cache_t& cache() const {
        if (!m_cache) m_cache.reset(new cache_t());
        return *m_cache;
    }

    void invalidate() noexcept { m_cache.reset(); }

    void normalize() {
        const bool bad = m_real.is_nan() || m_real.is_undefined() || m_imaginary.is_nan() || m_imaginary.is_undefined();
        if (!bad) return;
        if (!m_real.is_undefined())      m_real      = BigFloat::undefined();
        if (!m_imaginary.is_undefined()) m_imaginary = BigFloat::undefined();
    }

    void ensure_squares() const {
        cache_t& c = cache();
        if (c.have_sq) return;
        c.real_sq   = bcdetail::dy_square(m_real);
        c.imaginary_sq   = bcdetail::dy_square(m_imaginary);
        c.have_sq = true;
    }

    const bcdetail::dyadic& norm_cell(std::size_t K, bool& exact) const {
        cache_t& c = cache();

        if (!c.have_norm || (!c.norm_exact && c.norm_k < K)) {
            ensure_squares();
            bool ex = true;
            c.norm       = bcdetail::dy_sum(c.real_sq, c.imaginary_sq, K, RoundingMode::nearest_even, ex);
            c.norm_exact = ex;
            c.norm_k     = K;
            c.have_norm  = true;
        }

        exact = c.norm_exact;
        return c.norm;
    }

    const bcdetail::dyadic* exact_norm() const {
        bool ex = true;
        const bcdetail::dyadic& N = norm_cell(64, ex);
        if (ex) return bcdetail::dy_bad(N) ? nullptr : &N;
        cache_t& c = cache();
        bcdetail::dyadic X = bcdetail::dy_sum(c.real_sq, c.imaginary_sq, bcdetail::k_exact, RoundingMode::nearest_even, ex);
        if (bcdetail::dy_bad(X) || !ex) return nullptr;
        c.norm       = std::move(X);
        c.norm_exact = true;
        c.norm_k     = bcdetail::k_exact;
        return &c.norm;
    }

    BigComplex boxed() const {
        if (!is_infinite()) return BigComplex(m_real, m_imaginary);
        const BigFloat r = m_real.is_infinite() ? BigFloat::one(m_real.signbit()) : BigFloat::zero(m_real.signbit());
        const BigFloat i = m_imaginary.is_infinite() ? BigFloat::one(m_imaginary.signbit()) : BigFloat::zero(m_imaginary.signbit());
        return BigComplex(r, i);
    }

    static BigComplexContext sign_ctx() noexcept { return BigComplexContext(8); }

    bool argument_defined() const noexcept {
        return !is_undefined() && !is_zero() && !(m_real.is_infinite() && m_imaginary.is_infinite());
    }

    const BigFloat& principal_at(const BigFloatContext& wc) const {
        cache_t& c = cache();
        if (c.arg_w.hit(wc)) return c.arg_w.value;
        return c.arg_w.put(math::atan2(m_imaginary, m_real, wc), wc);
    }

    static BigComplex mul_finite(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) {
        const BigFloatContext cr = ctx.real(), ci = ctx.imaginary();
        const bcdetail::dyadic ac = bcdetail::dy_mul(z.m_real, w.m_real);
        const bcdetail::dyadic bd = bcdetail::dy_neg(bcdetail::dy_mul(z.m_imaginary, w.m_imaginary));
        const bcdetail::dyadic ad = bcdetail::dy_mul(z.m_real, w.m_imaginary);
        const bcdetail::dyadic bc = bcdetail::dy_mul(z.m_imaginary, w.m_real);
        bool ex = true;
        BigFloat re = bcdetail::dy_round(bcdetail::dy_sum(ac, bd, cr.precision + 8, cr.rounding_mode, ex), cr);
        BigFloat im = bcdetail::dy_round(bcdetail::dy_sum(ad, bc, ci.precision + 8, ci.rounding_mode, ex), ci);
        return BigComplex(std::move(re), std::move(im));
    }

    static BigComplex div_finite(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) {
        const BigFloatContext cr = ctx.real(), ci = ctx.imaginary();
        if (w.m_imaginary.is_zero()) return BigComplex(BigFloat::div(z.m_real, w.m_real, cr), BigFloat::div(z.m_imaginary, w.m_real, ci));
        if (w.m_real.is_zero()) return BigComplex(BigFloat::div(z.m_imaginary, w.m_imaginary, cr), BigFloat::div(z.m_real, -w.m_imaginary, ci));
        const bcdetail::dyadic ac = bcdetail::dy_mul(z.m_real, w.m_real);
        const bcdetail::dyadic bd = bcdetail::dy_mul(z.m_imaginary, w.m_imaginary);
        const bcdetail::dyadic bc = bcdetail::dy_mul(z.m_imaginary, w.m_real);
        const bcdetail::dyadic ad = bcdetail::dy_neg(bcdetail::dy_mul(z.m_real, w.m_imaginary));
        const RoundingMode rr = cr.rounding_mode, ri = ci.rounding_mode;
        auto den = [&w](std::size_t K, bool& ex) -> bcdetail::dyadic { return w.norm_cell(K, ex); };
        auto nre = [&](std::size_t K, bool& ex) { return bcdetail::dy_sum(ac, bd, K, rr, ex); };
        auto nim = [&](std::size_t K, bool& ex) { return bcdetail::dy_sum(bc, ad, K, ri, ex); };
        BigFloat re = bcdetail::dy_quotient(nre, den, cr);
        BigFloat im = bcdetail::dy_quotient(nim, den, ci);
        return BigComplex(std::move(re), std::move(im));
    }

    BigComplex reciprocal_finite(const BigComplexContext& ctx) const {
        const BigFloatContext cr = ctx.real(), ci = ctx.imaginary();
        if (m_imaginary.is_zero()) return BigComplex(m_real.reciprocal(cr), BigFloat::zero(!m_imaginary.signbit()));
        if (m_real.is_zero()) return BigComplex(BigFloat::zero(m_real.signbit()), (-m_imaginary).reciprocal(ci));
        const bcdetail::dyadic c = bcdetail::dy_of(m_real);
        const bcdetail::dyadic d = bcdetail::dy_neg(bcdetail::dy_of(m_imaginary));
        auto den = [this](std::size_t K, bool& ex) -> bcdetail::dyadic { return norm_cell(K, ex); };
        auto nre = [&c](std::size_t, bool& ex) { ex = true; return c; };
        auto nim = [&d](std::size_t, bool& ex) { ex = true; return d; };
        BigFloat re = bcdetail::dy_quotient(nre, den, cr);
        BigFloat im = bcdetail::dy_quotient(nim, den, ci);
        return BigComplex(std::move(re), std::move(im));
    }

    static void scale_slot(slot& s, std::int64_t n) {
        if (!s.valid) return;
        if (!s.value.is_finite() || s.value.is_zero()) { s.valid = false; return; }
        s.value.scale2_mutable(n);
    }

    static void scale_part(BigFloat& v, bool& ok, std::int64_t n) {
        if (v.is_zero()) return;
        if (!v.is_finite()) { ok = false; return; }
        v.scale2_mutable(n);
    }

public:
    BigComplex() = default;

    BigComplex(BigFloat re, BigFloat im = BigFloat()) : m_real(std::move(re)), m_imaginary(std::move(im)) { normalize(); }

    template <typename R, typename = typename std::enable_if<
            bcdetail::is_real_operand<R>::value && !std::is_same<typename std::decay<R>::type, BigFloat>::value
        >::type
    >
    BigComplex(const R& re) : m_real(re), m_imaginary() { normalize(); }

    template <typename T>
    BigComplex(const std::complex<T>& z) : m_real(z.real()), m_imaginary(z.imag()) { normalize(); }

    BigComplex(const BigComplex& o) : m_real(o.m_real), m_imaginary(o.m_imaginary), m_cache(o.m_cache ? new cache_t(*o.m_cache) : nullptr) {}
    BigComplex(BigComplex&&) = default;

    BigComplex& operator=(BigComplex o) noexcept {
        swap(o);
        return *this;
    }

    void swap(BigComplex& o) noexcept {
        bcdetail::swap_by_move(m_real, o.m_real);
        bcdetail::swap_by_move(m_imaginary, o.m_imaginary);
        m_cache.swap(o.m_cache);
    }

public:
    static BigComplex zero()      { return BigComplex(); }
    static BigComplex one()       { return BigComplex(BigFloat::one()); }
    static BigComplex i()         { return BigComplex(BigFloat::zero(), BigFloat::one()); }
    static BigComplex undefined() { return BigComplex(BigFloat::undefined(), BigFloat::undefined()); }

public:
    const BigFloat& real()      const noexcept { return m_real; }
    const BigFloat& imaginary() const noexcept { return m_imaginary; }

    void set_real(BigFloat v)          { invalidate(); m_real      = std::move(v); normalize(); }
    void set_imaginary(BigFloat v)     { invalidate(); m_imaginary = std::move(v); normalize(); }
    void set(BigFloat re, BigFloat im) { invalidate(); m_real      = std::move(re); m_imaginary = std::move(im); normalize(); }

    fpclass classify() const noexcept {
        if (m_real.is_nan() || m_real.is_undefined() || m_imaginary.is_nan() || m_imaginary.is_undefined()) return fpclass::undefined;
        if (m_real.is_infinite()  || m_imaginary.is_infinite()) return fpclass::infinite;
        return fpclass::finite;
    }

    bool is_nan()       const noexcept { return false; }
    bool is_undefined() const noexcept { return classify() == fpclass::undefined; }
    bool is_infinite()  const noexcept { return classify() == fpclass::infinite; }
    bool is_finite()    const noexcept { return m_real.is_finite() && m_imaginary.is_finite(); }
    bool is_special()   const noexcept { return !is_finite(); }
    bool is_zero()      const noexcept { return m_real.is_zero() && m_imaginary.is_zero(); }
    bool is_real()      const noexcept { return m_imaginary.is_zero(); }
    bool is_imaginary() const noexcept { return m_real.is_zero() && !m_imaginary.is_zero(); }

    bool has_cache()   const noexcept { return static_cast<bool>(m_cache); }
    void clear_cache() const noexcept { m_cache.reset(); }

public:
    BigFloat magnitude_squared(const BigFloatContext& ctx) const {
        switch (classify()) {
            case fpclass::undefined: return BigFloat::undefined();
            case fpclass::infinite:  return BigFloat::infinity();
            default: break;
        }

        if (is_zero()) return BigFloat::zero();
        cache_t& c = cache();
        if (c.norm_r.hit(ctx)) return c.norm_r.value;
        bool ex = true;
        const bcdetail::dyadic& N = norm_cell(ctx.precision + 8, ex);
        return c.norm_r.put(bcdetail::dy_round(N, ctx), ctx);
    }

    BigFloat magnitude_squared(const BigComplexContext& ctx) const { return magnitude_squared(ctx.scalar()); }
    BigFloat magnitude_squared() const { return magnitude_squared(BigComplexContext::current().scalar()); }

    BigFloat magnitude_squared_exact() const {
        switch (classify()) {
            case fpclass::undefined: return BigFloat::undefined();
            case fpclass::infinite:  return BigFloat::infinity();
            default: break;
        }

        const bcdetail::dyadic* N = exact_norm();
        return N ? bcdetail::dy_exact(*N) : BigFloat::undefined();
    }

    BigFloat magnitude(const BigFloatContext& ctx) const {
        switch (classify()) {
            case fpclass::undefined: return BigFloat::undefined();
            case fpclass::infinite:  return BigFloat::infinity();
            default: break;
        }

        if (m_imaginary.is_zero()) return m_real.abs().rounded(ctx);
        if (m_real.is_zero()) return m_imaginary.abs().rounded(ctx);
        cache_t& c = cache();
        if (c.abs_r.hit(ctx)) return c.abs_r.value;
        const std::size_t K  = 2 * ctx.precision + 16;
        bool              ex = true;
        const bcdetail::dyadic& N = norm_cell(K, ex);
        const bcdetail::dyadic  t = bcdetail::dy_truncate(N, K, ex);
        const std::int64_t top = bcdetail::dy_top(t);
        const std::int64_t q   = top + (top & 1);
        const BigFloat r = math::sqrt(BigFloat(t.mag, false, t.exp - q), ctx).scaled2(q / 2);
        return c.abs_r.put(r, ctx);
    }

    BigFloat magnitude(const BigComplexContext& ctx) const { return magnitude(ctx.scalar()); }
    BigFloat magnitude() const { return magnitude(BigComplexContext::current().scalar()); }

    BigFloat argument(const BigFloatContext& ctx) const {
        if (is_undefined()) return BigFloat::undefined();
        cache_t& c = cache();
        if (c.arg_r.hit(ctx)) return c.arg_r.value;
        return c.arg_r.put(math::atan2(m_imaginary, m_real, ctx), ctx);
    }

    BigFloat argument(const BigComplexContext& ctx) const { return argument(ctx.scalar()); }
    BigFloat argument() const { return argument(BigComplexContext::current().scalar()); }

    BigFloat argument(std::int64_t branch, const BigFloatContext& ctx) const {
        if (branch == 0) return argument(ctx);
        if (!argument_defined()) return BigFloat::undefined();
        cache_t& c = cache();
        if (c.branch_r.hit(ctx) && c.branch_k == branch) return c.branch_r.value;
        const std::int64_t    kb    = static_cast<std::int64_t>(bcdetail::bits_u64(bcdetail::abs_u64(branch)));
        const BigFloat        two_k = BigFloat(branch).scaled2(1);
        const BigFloatContext xc(BigFloatContext::max_prec, ctx.rounding_mode);
        std::size_t guard = 32;

        for (;;) {
            const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + guard), RoundingMode::nearest_even);
            const std::int64_t    w  = static_cast<std::int64_t>(wc.precision);
            const BigFloat        A  = principal_at(wc);
            if (!A.is_finite()) return BigFloat::undefined();
            const BigFloat     S     = BigFloat::add(A, BigFloat::mul(two_k, constants::pi(wc), xc), wc);
            const std::int64_t e_in  = kb + 3 - w;
            const std::int64_t e_out = S.get_exp_base2() - w;
            const BigFloat     delta(BigUInt::one(), false, (e_in > e_out ? e_in : e_out) + 1);
            const BigFloat     lo = BigFloat::sub(S, delta, xc).rounded(ctx);
            const BigFloat     hi = BigFloat::add(S, delta, xc).rounded(ctx);

            if (BigFloat::compare(lo, hi) == ordering::equal) {
                c.branch_k = branch;
                return c.branch_r.put(lo, ctx);
            }

            if (guard >= (std::size_t(1) << 16) || ctx.precision + guard >= BigFloatContext::max_prec / 4) return S.rounded(ctx);
            guard *= 2;
        }
    }

    BigFloat argument(std::int64_t branch, const BigComplexContext& ctx) const { return argument(branch, ctx.scalar()); }
    BigFloat argument(std::int64_t branch) const { return argument(branch, BigComplexContext::current().scalar()); }

    static ordering compare_argument(const BigComplex& a, const BigComplex& b) {
        if (!a.argument_defined() || !b.argument_defined()) return ordering::unordered;
        if (&a == &b) return ordering::equal;
        const BigComplex  ba = a.is_infinite() ? a.boxed() : BigComplex();
        const BigComplex  bb = b.is_infinite() ? b.boxed() : BigComplex();
        const BigComplex& da = a.is_infinite() ? ba : a;
        const BigComplex& db = b.is_infinite() ? bb : b;
        const int ca = bcdetail::argument_class(da.m_real, da.m_imaginary);
        const int cb = bcdetail::argument_class(db.m_real, db.m_imaginary);
        if (ca != cb) return (ca < cb) ? ordering::less : ordering::greater;
        if (ca != 1 && ca != 3) return ordering::equal;
        const int s = bcdetail::dy_sign_of_difference(bcdetail::dy_mul(da.m_real, db.m_imaginary), bcdetail::dy_mul(da.m_imaginary, db.m_real));
        return (s > 0) ? ordering::less : ((s < 0) ? ordering::greater : ordering::equal);
    }

    static ordering compare_argument(const BigComplex& a, const BigFloat& theta) {
        if (!a.argument_defined() || theta.is_nan() || theta.is_undefined()) return ordering::unordered;
        if (theta.is_infinite()) return theta.signbit() ? ordering::greater : ordering::less;
        const BigComplex  ba = a.is_infinite() ? a.boxed() : BigComplex();
        const BigComplex& da = a.is_infinite() ? ba : a;
        if (bcdetail::argument_class(da.m_real, da.m_imaginary) == 2) return BigFloat::compare(BigFloat::zero(), theta);
        const BigFloat four(4);
        if (theta > four)  return ordering::less;
        if (theta < -four) return ordering::greater;
        const BigFloatContext xc(BigFloatContext::max_prec);
        std::size_t w = 64;

        for (;;) {
            const BigFloatContext wc(BigFloatContext::clamp_precision(w), RoundingMode::nearest_even);
            const BigFloat A = a.principal_at(wc);
            if (!A.is_finite() || A.is_zero()) return ordering::unordered;
            const BigFloat err(BigUInt::one(), false, A.get_exp_base2() + 1 - static_cast<std::int64_t>(wc.precision));
            if (theta < BigFloat::sub(A, err, xc)) return ordering::greater;
            if (theta > BigFloat::add(A, err, xc)) return ordering::less;
            if (w >= (std::size_t(1) << 20) || w >= BigFloatContext::max_prec / 4) return ordering::unordered;
            w *= 2;
        }
    }

    ordering compare_argument(const BigComplex& o)   const { return compare_argument(*this, o); }
    ordering compare_argument(const BigFloat& theta) const { return compare_argument(*this, theta); }

    static ordering compare_magnitude(const BigComplex& a, const BigComplex& b) {
        if (a.is_undefined() || b.is_undefined()) return ordering::unordered;
        if (&a == &b) return ordering::equal;
        const bool ai = a.is_infinite(), bi = b.is_infinite();
        if (ai || bi) return (ai && bi) ? ordering::equal : (ai ? ordering::greater : ordering::less);
        bool ea = true, eb = true;
        int  c  = 0;
        const bcdetail::dyadic& na = a.norm_cell(64, ea);
        const bcdetail::dyadic& nb = b.norm_cell(64, eb);
        if (bcdetail::dy_order(na, ea, nb, eb, c)) return bcdetail::to_ordering(c);
        const bcdetail::dyadic* xa = a.exact_norm();
        const bcdetail::dyadic* xb = b.exact_norm();
        if (!xa || !xb) return ordering::unordered;
        return bcdetail::to_ordering(bcdetail::dy_cmp_abs(*xa, *xb));
    }

    static ordering compare_magnitude(const BigComplex& a, const BigFloat& r) {
        if (a.is_undefined() || r.is_nan() || r.is_undefined()) return ordering::unordered;
        if (r.is_negative() && !r.is_zero()) return ordering::greater;
        if (r.is_infinite()) return a.is_infinite() ? ordering::equal : ordering::less;
        if (a.is_infinite()) return ordering::greater;
        const bcdetail::dyadic rr = bcdetail::dy_square(r);
        bool ea = true;
        int  c  = 0;
        const bcdetail::dyadic& na = a.norm_cell(64, ea);
        if (bcdetail::dy_order(na, ea, rr, true, c)) return bcdetail::to_ordering(c);
        const bcdetail::dyadic* xa = a.exact_norm();
        if (!xa) return ordering::unordered;
        return bcdetail::to_ordering(bcdetail::dy_cmp_abs(*xa, rr));
    }

    ordering compare_magnitude(const BigComplex& o) const { return compare_magnitude(*this, o); }
    ordering compare_magnitude(const BigFloat& r)   const { return compare_magnitude(*this, r); }

public:
    BigComplex& conjugate_mutable() {
        m_imaginary.negate_mutable();

        if (m_cache) {
            cache_t& c = *m_cache;
            if (c.arg_r.valid) { if (bcdetail::symmetric(c.arg_r.mode)) c.arg_r.value.negate_mutable(); else c.arg_r.valid = false; }
            if (c.arg_w.valid) c.arg_w.value.negate_mutable();
            c.branch_r.valid = false;
            if (c.recip.valid) { if (bcdetail::symmetric(c.recip.ctx.imaginary_rounding)) c.recip.im.negate_mutable(); else c.recip.valid = false; }
        }

        return *this;
    }

    BigComplex& negate_mutable() {
        m_real.negate_mutable();
        m_imaginary.negate_mutable();

        if (m_cache) {
            cache_t& c = *m_cache;
            c.arg_r.valid    = false;
            c.arg_w.valid    = false;
            c.branch_r.valid = false;

            if (c.recip.valid) {
                if (bcdetail::symmetric(c.recip.ctx.real_rounding) && bcdetail::symmetric(c.recip.ctx.imaginary_rounding)) {
                    c.recip.re.negate_mutable();
                    c.recip.im.negate_mutable();
                } else {
                    c.recip.valid = false;
                }
            }
        }

        return *this;
    }

    BigComplex& mul_i_mutable() {
        BigFloat t = std::move(m_real);
        m_real = std::move(m_imaginary);
        m_real.negate_mutable();
        m_imaginary = std::move(t);

        if (m_cache) {
            cache_t& c = *m_cache;
            bcdetail::swap_by_move(c.real_sq, c.imaginary_sq);
            c.arg_r.valid    = false;
            c.arg_w.valid    = false;
            c.branch_r.valid = false;
            c.recip.valid = false;
        }

        return *this;
    }

    BigComplex& div_i_mutable() {
        BigFloat t = std::move(m_imaginary);
        m_imaginary = std::move(m_real);
        m_imaginary.negate_mutable();
        m_real = std::move(t);

        if (m_cache) {
            cache_t& c = *m_cache;
            bcdetail::swap_by_move(c.real_sq, c.imaginary_sq);
            c.arg_r.valid    = false;
            c.arg_w.valid    = false;
            c.branch_r.valid = false;
            c.recip.valid = false;
        }

        return *this;
    }

    BigComplex& scale2_mutable(std::int64_t n) {
        if (n == 0) return *this;
        const bool fin = is_finite();
        const bool rz  = m_real.is_zero(), iz = m_imaginary.is_zero();
        m_real.scale2_mutable(n);
        m_imaginary.scale2_mutable(n);
        if (!m_cache) return *this;

        if (!fin || !is_finite() || m_real.is_zero() != rz || m_imaginary.is_zero() != iz) {
            invalidate();
            return *this;
        }

        cache_t& c = *m_cache;

        if (c.have_sq) {
            if (!bcdetail::dy_zero(c.real_sq)) c.real_sq.exp += 2 * n;
            if (!bcdetail::dy_zero(c.imaginary_sq)) c.imaginary_sq.exp += 2 * n;
        }

        if (c.have_norm && !bcdetail::dy_zero(c.norm)) c.norm.exp += 2 * n;
        scale_slot(c.norm_r, 2 * n);
        scale_slot(c.abs_r, n);

        if (c.recip.valid) {
            bool ok = true;
            scale_part(c.recip.re, ok, -n);
            scale_part(c.recip.im, ok, -n);
            if (!ok) c.recip.valid = false;
        }

        return *this;
    }

    BigComplex conjugate()             const { BigComplex r(*this); r.conjugate_mutable(); return r; }
    BigComplex negated()               const { BigComplex r(*this); r.negate_mutable();    return r; }
    BigComplex mul_i()                 const { BigComplex r(*this); r.mul_i_mutable();     return r; }
    BigComplex div_i()                 const { BigComplex r(*this); r.div_i_mutable();     return r; }
    BigComplex scaled2(std::int64_t n) const { BigComplex r(*this); r.scale2_mutable(n);   return r; }

    BigComplex rounded(const BigComplexContext& ctx) const {
        if (m_real.significand_bits() <= ctx.real_precision && m_imaginary.significand_bits() <= ctx.imaginary_precision) return *this;
        return BigComplex(m_real.rounded(ctx.real()), m_imaginary.rounded(ctx.imaginary()));
    }

    BigComplex rounded() const { return rounded(BigComplexContext::current()); }

public:
    BigComplex square(const BigComplexContext& ctx) const {
        if (is_undefined()) return undefined();
        if (is_infinite())  { const BigComplex t(*this); return mul(*this, t, ctx); }
        const BigFloatContext cr = ctx.real(), ci = ctx.imaginary();
        ensure_squares();
        const cache_t& c = *m_cache;
        bool ex = true;
        BigFloat re = bcdetail::dy_round(bcdetail::dy_sum(c.real_sq, bcdetail::dy_neg(c.imaginary_sq), cr.precision + 8, cr.rounding_mode, ex), cr);
        BigFloat im = bcdetail::dy_round(bcdetail::dy_twice(bcdetail::dy_mul(m_real, m_imaginary)), ci);
        return BigComplex(std::move(re), std::move(im));
    }

    BigComplex square() const { return square(BigComplexContext::current()); }

    BigComplex reciprocal(const BigComplexContext& ctx) const {
        if (!is_finite() || is_zero()) return div(one(), *this, ctx);
        cache_t& c = cache();
        if (c.recip.valid && c.recip.ctx == ctx) return BigComplex(c.recip.re, c.recip.im);
        BigComplex r = reciprocal_finite(ctx);
        c.recip.re    = r.m_real;
        c.recip.im    = r.m_imaginary;
        c.recip.ctx   = ctx;
        c.recip.valid = true;
        return r;
    }

    BigComplex reciprocal() const { return reciprocal(BigComplexContext::current()); }

public:
    static BigComplex add(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) {
        return BigComplex(BigFloat::add(z.m_real, w.m_real, ctx.real()), BigFloat::add(z.m_imaginary, w.m_imaginary, ctx.imaginary()));
    }

    static BigComplex sub(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) {
        return BigComplex(BigFloat::sub(z.m_real, w.m_real, ctx.real()), BigFloat::sub(z.m_imaginary, w.m_imaginary, ctx.imaginary()));
    }

    static BigComplex mul(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) {
        if (z.is_undefined() || w.is_undefined()) return undefined();

        if (z.is_infinite() || w.is_infinite()) {
            if (z.is_zero() || w.is_zero()) return undefined();
            const BigComplex s = mul_finite(z.boxed(), w.boxed(), sign_ctx());
            return BigComplex(bcdetail::to_infinity(s.m_real), bcdetail::to_infinity(s.m_imaginary));
        }

        if (&z == &w) return z.square(ctx);
        return mul_finite(z, w, ctx);
    }

    static BigComplex div(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) {
        if (z.is_undefined() || w.is_undefined()) return undefined();
        const bool zi = z.is_infinite(), wi = w.is_infinite();
        if (zi && wi) return undefined();

        if (w.is_zero()) {
            if (z.is_zero()) return undefined();
            return BigComplex(bcdetail::to_infinity(z.m_real), bcdetail::to_infinity(z.m_imaginary));
        }

        if (zi) {
            const BigComplex s = div_finite(z.boxed(), w, sign_ctx());
            return BigComplex(bcdetail::to_infinity(s.m_real), bcdetail::to_infinity(s.m_imaginary));
        }

        if (wi) {
            const BigComplex s = div_finite(z, w.boxed(), sign_ctx());
            return BigComplex(bcdetail::to_zero(s.m_real), bcdetail::to_zero(s.m_imaginary));
        }

        if (&z == &w) return BigComplex(BigFloat::one(), BigFloat::zero(ctx.imaginary_rounding == RoundingMode::toward_neg_inf));
        return div_finite(z, w, ctx);
    }

    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>
    static BigComplex add(const BigComplex& z, const R& x, const BigComplexContext& ctx) {
        const BigFloat& r = bcdetail::as_float(x);
        return BigComplex(BigFloat::add(z.m_real, r, ctx.real()), z.m_imaginary.rounded(ctx.imaginary()));
    }

    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>
    static BigComplex add(const R& x, const BigComplex& z, const BigComplexContext& ctx) {
        const BigFloat& r = bcdetail::as_float(x);
        return BigComplex(BigFloat::add(r, z.m_real, ctx.real()), z.m_imaginary.rounded(ctx.imaginary()));
    }

    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>
    static BigComplex sub(const BigComplex& z, const R& x, const BigComplexContext& ctx) {
        const BigFloat& r = bcdetail::as_float(x);
        return BigComplex(BigFloat::sub(z.m_real, r, ctx.real()), z.m_imaginary.rounded(ctx.imaginary()));
    }

    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>
    static BigComplex sub(const R& x, const BigComplex& z, const BigComplexContext& ctx) {
        const BigFloat& r = bcdetail::as_float(x);
        return BigComplex(BigFloat::sub(r, z.m_real, ctx.real()), (-z.m_imaginary).rounded(ctx.imaginary()));
    }

    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>
    static BigComplex mul(const BigComplex& z, const R& x, const BigComplexContext& ctx) {
        const BigFloat& r = bcdetail::as_float(x);
        return BigComplex(BigFloat::mul(z.m_real, r, ctx.real()), BigFloat::mul(z.m_imaginary, r, ctx.imaginary()));
    }

    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>
    static BigComplex mul(const R& x, const BigComplex& z, const BigComplexContext& ctx) { return mul(z, x, ctx); }

    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>
    static BigComplex div(const BigComplex& z, const R& x, const BigComplexContext& ctx) {
        const BigFloat& r = bcdetail::as_float(x);
        return BigComplex(BigFloat::div(z.m_real, r, ctx.real()), BigFloat::div(z.m_imaginary, r, ctx.imaginary()));
    }

    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>
    static BigComplex div(const R& x, const BigComplex& w, const BigComplexContext& ctx) {
        return div(BigComplex(bcdetail::as_float(x)), w, ctx);
    }

#define FIZMO_BIGCOMPLEX_DEFAULT_CTX(OP)                                                                             \
    static BigComplex OP(const BigComplex& a, const BigComplex& b) { return OP(a, b, BigComplexContext::current()); } \
    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>             \
    static BigComplex OP(const BigComplex& a, const R& b) { return OP(a, b, BigComplexContext::current()); }          \
    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>             \
    static BigComplex OP(const R& a, const BigComplex& b) { return OP(a, b, BigComplexContext::current()); }

    FIZMO_BIGCOMPLEX_DEFAULT_CTX(add)
    FIZMO_BIGCOMPLEX_DEFAULT_CTX(sub)
    FIZMO_BIGCOMPLEX_DEFAULT_CTX(mul)
    FIZMO_BIGCOMPLEX_DEFAULT_CTX(div)

#undef FIZMO_BIGCOMPLEX_DEFAULT_CTX

#define FIZMO_BIGCOMPLEX_MUTABLE(OP)                                                                                 \
    BigComplex& OP##_mutable(const BigComplex& o, const BigComplexContext& c) { return *this = OP(*this, o, c); }    \
    BigComplex& OP##_mutable(const BigComplex& o) { return *this = OP(*this, o, BigComplexContext::current()); }     \
    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>             \
    BigComplex& OP##_mutable(const R& o, const BigComplexContext& c) { return *this = OP(*this, o, c); }             \
    template <typename R, typename = typename std::enable_if<bcdetail::is_real_operand<R>::value>::type>             \
    BigComplex& OP##_mutable(const R& o) { return *this = OP(*this, o, BigComplexContext::current()); }

    FIZMO_BIGCOMPLEX_MUTABLE(add)
    FIZMO_BIGCOMPLEX_MUTABLE(sub)
    FIZMO_BIGCOMPLEX_MUTABLE(mul)
    FIZMO_BIGCOMPLEX_MUTABLE(div)

#undef FIZMO_BIGCOMPLEX_MUTABLE

    BigComplex& square_mutable(const BigComplexContext& c)     { return *this = square(c); }
    BigComplex& square_mutable()                               { return *this = square(); }
    BigComplex& reciprocal_mutable(const BigComplexContext& c) { return *this = reciprocal(c); }
    BigComplex& reciprocal_mutable()                           { return *this = reciprocal(); }

public:
    std::string to_string(std::size_t max_digits = BigFloat::no_digit_limit, std::int64_t scientific_notation_exp = -5, std::size_t digits = 0) const {
        const BigComplexContext cc = BigComplexContext::current();
        const std::size_t dr = digits ? digits : BigFloatContext::dps_from_precision(cc.real_precision);
        const std::size_t di = digits ? digits : BigFloatContext::dps_from_precision(cc.imaginary_precision);
        return "(" + m_real.to_string(max_digits, scientific_notation_exp, dr) + ", " + m_imaginary.to_string(max_digits, scientific_notation_exp, di) + ")";
    }

    friend std::ostream& operator<<(std::ostream& os, const BigComplex& z) { return os << z.to_string(); }
};

inline void swap(BigComplex& a, BigComplex& b) noexcept { a.swap(b); }

namespace bcdetail {
template <typename C>
struct is_complex_arg : std::is_same<typename std::decay<C>::type, BigComplex> {};

template <typename C, typename R>
struct complex_real_pair : std::integral_constant<bool, is_complex_arg<C>::value && is_real_operand<R>::value> {};

} // namespace bcdetail

inline bool operator==(const BigComplex& a, const BigComplex& b) noexcept { return a.real() == b.real() && a.imaginary() == b.imaginary(); }
inline bool operator!=(const BigComplex& a, const BigComplex& b) noexcept { return !(a == b); }

template <typename C, typename R, typename std::enable_if<bcdetail::complex_real_pair<C, R>::value, int>::type = 0>
inline bool operator==(const C& a, const R& b) { return a.imaginary().is_zero() && a.real() == bcdetail::as_float(b); }

template <typename R, typename C, typename std::enable_if<bcdetail::complex_real_pair<C, R>::value, int>::type = 0>
inline bool operator==(const R& a, const C& b) { return b == a; }

template <typename C, typename R, typename std::enable_if<bcdetail::complex_real_pair<C, R>::value, int>::type = 0>
inline bool operator!=(const C& a, const R& b) { return !(a == b); }

template <typename R, typename C, typename std::enable_if<bcdetail::complex_real_pair<C, R>::value, int>::type = 0>
inline bool operator!=(const R& a, const C& b) { return !(b == a); }

inline BigComplex operator+(const BigComplex& a) { return a; }
inline BigComplex operator+(BigComplex&& a)      { return std::move(a); }
inline BigComplex operator-(const BigComplex& a) { return a.negated(); }
inline BigComplex operator-(BigComplex&& a)      { a.negate_mutable(); return std::move(a); }

#define FIZMO_BIGCOMPLEX_BINOP(OP, NAME)                                                                             \
    inline BigComplex operator OP(const BigComplex& a, const BigComplex& b) { return BigComplex::NAME(a, b); }       \
    template <typename C, typename R, typename std::enable_if<bcdetail::complex_real_pair<C, R>::value, int>::type = 0> \
    inline BigComplex operator OP(const C& a, const R& b) { return BigComplex::NAME(a, b); }                         \
    template <typename R, typename C, typename std::enable_if<bcdetail::complex_real_pair<C, R>::value, int>::type = 0> \
    inline BigComplex operator OP(const R& a, const C& b) { return BigComplex::NAME(a, b); }                         \
    inline BigComplex& operator OP##=(BigComplex& a, const BigComplex& b) { return a.NAME##_mutable(b); }            \
    template <typename R, typename std::enable_if<bcdetail::is_real_operand<R>::value, int>::type = 0>               \
    inline BigComplex& operator OP##=(BigComplex& a, const R& b) { return a.NAME##_mutable(b); }

FIZMO_BIGCOMPLEX_BINOP(+, add)
FIZMO_BIGCOMPLEX_BINOP(-, sub)
FIZMO_BIGCOMPLEX_BINOP(*, mul)
FIZMO_BIGCOMPLEX_BINOP(/, div)

#undef FIZMO_BIGCOMPLEX_BINOP

namespace math {
namespace bcdetail_fn {
template <typename C> using if_complex = typename std::enable_if<std::is_same<C, BigComplex>::value, int>::type;
} // namespace bcdetail_fn

#define FIZMO_BIGCOMPLEX_SCALAR_FN(NAME, METHOD)                                                                     \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                            \
    inline BigFloat NAME(const C& z, const BigFloatContext& c)   { return z.METHOD(c); }                             \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                            \
    inline BigFloat NAME(const C& z, const BigComplexContext& c) { return z.METHOD(c); }                             \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                            \
    inline BigFloat NAME(const C& z)                             { return z.METHOD(); }

FIZMO_BIGCOMPLEX_SCALAR_FN(magnitude_squared, magnitude_squared)
FIZMO_BIGCOMPLEX_SCALAR_FN(norm,              magnitude_squared)
FIZMO_BIGCOMPLEX_SCALAR_FN(magnitude,         magnitude)
FIZMO_BIGCOMPLEX_SCALAR_FN(abs,               magnitude)
FIZMO_BIGCOMPLEX_SCALAR_FN(argument,          argument)
FIZMO_BIGCOMPLEX_SCALAR_FN(arg,               argument)

#undef FIZMO_BIGCOMPLEX_SCALAR_FN

#define FIZMO_BIGCOMPLEX_BRANCH_FN(NAME)                                                                             \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                            \
    inline BigFloat NAME(const C& z, std::int64_t branch, const BigFloatContext& c)   { return z.argument(branch, c); } \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                            \
    inline BigFloat NAME(const C& z, std::int64_t branch, const BigComplexContext& c) { return z.argument(branch, c); } \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                            \
    inline BigFloat NAME(const C& z, std::int64_t branch)                             { return z.argument(branch); }

FIZMO_BIGCOMPLEX_BRANCH_FN(argument)
FIZMO_BIGCOMPLEX_BRANCH_FN(arg)

#undef FIZMO_BIGCOMPLEX_BRANCH_FN

template <typename C, bcdetail_fn::if_complex<C> = 0> inline bool is_nan(const C&)         { return false; }
template <typename C, bcdetail_fn::if_complex<C> = 0> inline bool is_undefined(const C& z) { return z.is_undefined(); }
template <typename C, bcdetail_fn::if_complex<C> = 0> inline bool is_infinite(const C& z)  { return z.is_infinite(); }
template <typename C, bcdetail_fn::if_complex<C> = 0> inline bool is_finite(const C& z)    { return z.is_finite(); }
template <typename C, bcdetail_fn::if_complex<C> = 0> inline bool is_zero(const C& z)      { return z.is_zero(); }

template <typename C, bcdetail_fn::if_complex<C> = 0> inline const BigFloat& real(const C& z) { return z.real(); }
template <typename C, bcdetail_fn::if_complex<C> = 0> inline const BigFloat& imaginary(const C& z) { return z.imaginary(); }
template <typename C, bcdetail_fn::if_complex<C> = 0> inline BigComplex conjugate(const C& z)      { return z.conjugate(); }

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex reciprocal(const C& z, const BigComplexContext& c) { return z.reciprocal(c); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex reciprocal(const C& z) { return z.reciprocal(); }

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex square(const C& z, const BigComplexContext& c) { return z.square(c); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex square(const C& z) { return z.square(); }

inline BigFloat::ordering compare_magnitude(const BigComplex& a, const BigComplex& b) { return BigComplex::compare_magnitude(a, b); }
inline BigFloat::ordering compare_magnitude(const BigComplex& a, const BigFloat& r)   { return BigComplex::compare_magnitude(a, r); }
inline BigFloat::ordering compare_argument(const BigComplex& a, const BigComplex& b)  { return BigComplex::compare_argument(a, b); }
inline BigFloat::ordering compare_argument(const BigComplex& a, const BigFloat& t)    { return BigComplex::compare_argument(a, t); }

} // namespace math

} // namespace multiprecision

template <class>  struct is_fizmo_big_complex : std::false_type {};
template <> struct is_fizmo_big_complex<multiprecision::BigComplex> : std::true_type {};
template <class T> constexpr bool is_fizmo_big_complex_v = is_fizmo_big_complex<T>::value;

} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_COMPLEX_HPP