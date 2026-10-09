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

    BigComplexContext extended(std::size_t guard_bits) const noexcept;

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
;

    ScopedComplexContext(std::size_t prec, RoundingMode rnd) noexcept : ScopedComplexContext(BigComplexContext(prec, rnd)) {}

    ScopedComplexContext(std::size_t p_re, std::size_t p_im) noexcept
;

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

 dyadic dy_of(const BF& x);

inline dyadic dy_neg(dyadic d) { d.neg = !d.neg; return d; }

 dyadic dy_mul(const BF& x, const BF& y);

 dyadic dy_square(const BF& x);

inline dyadic dy_twice(dyadic d) { if (!dy_zero(d)) ++d.exp; return d; }

inline BF dy_unit(const dyadic& d) { return BF(d.mag, d.neg, -dy_len(d)); }

 BF dy_round(const dyadic& d, const BFC& ctx);

 BF dy_exact(const dyadic& d);

 dyadic dy_add_exact(const dyadic& a, const dyadic& b);

 dyadic dy_sum(const dyadic& a, const dyadic& b, std::size_t K, RoundingMode mode, bool& exact);

 dyadic dy_truncate(const dyadic& d, std::size_t K, bool& exact);

 BF dy_divide(const dyadic& n, const dyadic& d, const BFC& ctx);

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

 int dy_cmp_abs(const dyadic& x, const dyadic& y);

 bool dy_order(const dyadic& a, bool a_exact, const dyadic& b, bool b_exact, int& out);

inline std::size_t bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline std::uint64_t abs_u64(std::int64_t v) noexcept {
    return (v < 0) ? (~static_cast<std::uint64_t>(v) + 1u) : static_cast<std::uint64_t>(v);
}

inline int dy_sign(const dyadic& d) noexcept { return dy_zero(d) ? 0 : (d.neg ? -1 : 1); }

 int dy_sign_of_difference(const dyadic& p, const dyadic& q);

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

 bool symmetric(RoundingMode r) noexcept;

inline BF to_infinity(const BF& s) { return s.is_zero() ? s : BF::infinity(s.signbit()); }
inline BF to_zero(const BF& s)     { return BF::zero(s.signbit()); }

 BigFloat::ordering to_ordering(int c) noexcept;

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

        cache_t();
    };

    BigFloat m_real;
    BigFloat m_imaginary;
    mutable std::unique_ptr<cache_t> m_cache;

    cache_t& cache() const {
        if (!m_cache) m_cache.reset(new cache_t());
        return *m_cache;
    }

    void invalidate() noexcept { m_cache.reset(); }

    void normalize();

    void ensure_squares() const;

    const bcdetail::dyadic& norm_cell(std::size_t K, bool& exact) const;

    const bcdetail::dyadic* exact_norm() const;

    BigComplex boxed() const;

    static BigComplexContext sign_ctx() noexcept { return BigComplexContext(8); }

    bool argument_defined() const noexcept {
        return !is_undefined() && !is_zero() && !(m_real.is_infinite() && m_imaginary.is_infinite());
    }

    const BigFloat& principal_at(const BigFloatContext& wc) const;

    static BigComplex mul_finite(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx);

    static BigComplex div_finite(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx);

    BigComplex reciprocal_finite(const BigComplexContext& ctx) const;

    static void scale_slot(slot& s, std::int64_t n);

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

    BigComplex(const BigComplex& o);
    BigComplex(BigComplex&&) = default;

    BigComplex& operator=(BigComplex o) noexcept {
        swap(o);
        return *this;
    }

    void swap(BigComplex& o) noexcept;

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

    fpclass classify() const noexcept;

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
    BigFloat magnitude_squared(const BigFloatContext& ctx) const;

    BigFloat magnitude_squared(const BigComplexContext& ctx) const { return magnitude_squared(ctx.scalar()); }
    BigFloat magnitude_squared() const { return magnitude_squared(BigComplexContext::current().scalar()); }

    BigFloat magnitude_squared_exact() const;

    BigFloat magnitude(const BigFloatContext& ctx) const;

    BigFloat magnitude(const BigComplexContext& ctx) const { return magnitude(ctx.scalar()); }
    BigFloat magnitude() const { return magnitude(BigComplexContext::current().scalar()); }

    BigFloat argument(const BigFloatContext& ctx) const;

    BigFloat argument(const BigComplexContext& ctx) const { return argument(ctx.scalar()); }
    BigFloat argument() const { return argument(BigComplexContext::current().scalar()); }

    BigFloat argument(std::int64_t branch, const BigFloatContext& ctx) const;

    BigFloat argument(std::int64_t branch, const BigComplexContext& ctx) const { return argument(branch, ctx.scalar()); }
    BigFloat argument(std::int64_t branch) const { return argument(branch, BigComplexContext::current().scalar()); }

    static ordering compare_argument(const BigComplex& a, const BigComplex& b);

    static ordering compare_argument(const BigComplex& a, const BigFloat& theta);

    ordering compare_argument(const BigComplex& o)   const { return compare_argument(*this, o); }
    ordering compare_argument(const BigFloat& theta) const { return compare_argument(*this, theta); }

    static ordering compare_magnitude(const BigComplex& a, const BigComplex& b);

    static ordering compare_magnitude(const BigComplex& a, const BigFloat& r);

    ordering compare_magnitude(const BigComplex& o) const { return compare_magnitude(*this, o); }
    ordering compare_magnitude(const BigFloat& r)   const { return compare_magnitude(*this, r); }

public:
    BigComplex& conjugate_mutable();

    BigComplex& negate_mutable();

    BigComplex& mul_i_mutable();

    BigComplex& div_i_mutable();

    BigComplex& scale2_mutable(std::int64_t n);

    BigComplex conjugate()             const { BigComplex r(*this); r.conjugate_mutable(); return r; }
    BigComplex negated()               const { BigComplex r(*this); r.negate_mutable();    return r; }
    BigComplex mul_i()                 const { BigComplex r(*this); r.mul_i_mutable();     return r; }
    BigComplex div_i()                 const { BigComplex r(*this); r.div_i_mutable();     return r; }
    BigComplex scaled2(std::int64_t n) const { BigComplex r(*this); r.scale2_mutable(n);   return r; }

    BigComplex rounded(const BigComplexContext& ctx) const;

    BigComplex rounded() const { return rounded(BigComplexContext::current()); }

public:
    BigComplex square(const BigComplexContext& ctx) const;

    BigComplex square() const { return square(BigComplexContext::current()); }

    BigComplex reciprocal(const BigComplexContext& ctx) const;

    BigComplex reciprocal() const { return reciprocal(BigComplexContext::current()); }

public:
    static BigComplex add(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx);

    static BigComplex sub(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx);

    static BigComplex mul(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx);

    static BigComplex div(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx);

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
    std::string to_string(std::size_t max_digits = BigFloat::no_digit_limit, std::int64_t scientific_notation_exp = -5, std::size_t digits = 0) const;

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