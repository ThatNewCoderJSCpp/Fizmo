#ifndef FIZMO_MULTIPRECISION_NORMAL_TRIG_DETAIL_HPP
#define FIZMO_MULTIPRECISION_NORMAL_TRIG_DETAIL_HPP

#include "log_pow_roots_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace edetail {

constexpr std::size_t TRIG_HEADROOM  = 1 << 8;    // exponent headroom of the cheap path
constexpr std::size_t TRIG_MAX_TOTAL = 1 << 13;   // hard cap on the wide path's headroom
constexpr long long   TRIG_POW3_MAX  = 20;        // 3^20 < 2^32, so div_u32 still applies

constexpr std::size_t next_pow2(std::size_t n, std::size_t p = 128) noexcept {
    return (p >= n) ? p : next_pow2(n, p * 2);
}

constexpr std::size_t trig_span(std::size_t exp_bits) noexcept {
    return (exp_bits >= 20) ? (std::size_t(1) << 19) : ((std::size_t(1) << (exp_bits - 1)) - 1u);
}

template <class F, std::size_t Extra>
struct reduce_of {
    using G = typename F::guard_t;
    static constexpr std::size_t base  = F::mantissa_bits + G::mantissa_bits + 96;
    static constexpr std::size_t need  = next_pow2(base + 128);
    static constexpr std::size_t want  = next_pow2(base + Extra + 128);
    static constexpr std::size_t total = (want <= TRIG_MAX_TOTAL) ? want : ((need > TRIG_MAX_TOTAL) ? need : TRIG_MAX_TOTAL);
    using type = floatmp<total, total - 64, sign::is_signed>;
};

template <class F> using trig_fast_t = typename reduce_of<F, TRIG_HEADROOM>::type;
template <class F> using trig_wide_t = typename reduce_of<F, trig_span(F::exponent_bits)>::type;

template <class G>
struct trig_arg {
    G        r;
    G        rc;
    unsigned q;
    bool     neg;
    bool     ok;
};

template <class W, class G>
bool trig_reduce_in(const G& xa, long long e0, long long fmant, trig_arg<G>& a) noexcept {
    const long long lead = (e0 > fmant) ? e0 : fmant;
    if (lead + static_cast<long long>(G::mantissa_bits) + 32 > static_cast<long long>(W::mantissa_bits)) return false;
    const W  xw = fmath::float_cast<W>(xa);
    const long long ex = e0 - fmant;
    const W  xi = fmath::scalb_i(xw, -ex);                       
    const W  w  = fmath::scalb_i(fmath::scalb_i(constants::detail::two_inv_pi_c<W>(), ex - 2).get_fractional_part(), 2);            
    const W  t  = xi * w;                                        
    const W  ip = t.get_integer_part();
    const W  f  = t - ip;                                        
    const W& hp = constants::detail::half_pi_c<W>();
    a.q  = static_cast<unsigned>(ip.get_integer_part_as_int().get_lowest_bits() & 3ull);
    a.r  = fmath::float_cast<G>(f * hp);
    a.rc = fmath::float_cast<G>((W(1) - f) * hp);
    return true;
}

template <class F>
trig_arg<typename F::guard_t> trig_reduce(const F& x) noexcept {
    using G = typename F::guard_t;
    trig_arg<G> a;
    a.q = 0; a.neg = x.is_negative(); a.ok = true;
    const G  xa = fmath::float_cast<G>(math::abs(x));
    const G& hp = constants::detail::half_pi_c<G>();
    if (xa.is_zero())                      { a.r = G::zero(); a.rc = hp; return a; }
    if (xa < fmath::scalb_i(hp, -1))       { a.r = xa; a.rc = hp - xa;   return a; }  
    const long long e0 = root_exp_ll<G>(xa.exponent_base2());
    const long long fm = static_cast<long long>(F::mantissa_bits);
    if (trig_reduce_in<trig_fast_t<F>>(xa, e0, fm, a)) return a;
    if (trig_reduce_in<trig_wide_t<F>>(xa, e0, fm, a)) return a;
    a.ok = false;
    return a;
}

template <class G>
G sin_series(const G& t) noexcept {
    using S = typename G::sstore_t;
    if (t.is_zero()) return t;
    const long long P = static_cast<long long>(G::mantissa_bits);
    const S tiny = t.exponent_base2() - S(P + 8);
    const G t2 = t * t;
    G term(t), sum(t);
    bool sub = true;

    for (long long n = 3; n <= 2 * P + 16; n += 2) {
        term = div_ll(term * t2, n * (n - 1));
        if (term.is_zero()) break;
        sum  = sub ? (sum - term) : (sum + term);
        sub  = !sub;
        if (term.exponent_base2() < tiny) break;
    }

    return sum;
}

template <class G>
G sin_reduced(const G& a) noexcept {
    if (a.is_zero()) return G::zero();
    const long long P = static_cast<long long>(G::mantissa_bits);
    long long m = isqrt_ll(P / 6);
    if (m > TRIG_POW3_MAX) m = TRIG_POW3_MAX;
    std::uint32_t p = 1u;
    for (long long i = 0; i < m; ++i) p *= 3u;
    G s = sin_series(m ? div_u32(a, p) : a);
    const G three(3);
    for (long long i = 0; i < m; ++i) s = s * (three - fmath::scalb_i(s * s, 2));
    return s;
}

template <class G>
G sin_from(const trig_arg<G>& a) noexcept {
    switch (a.q) {
        case 0:  return sin_reduced(a.r);
        case 1:  return sin_reduced(a.rc);
        case 2:  return G::zero() - sin_reduced(a.r);
        default: return G::zero() - sin_reduced(a.rc);
    }
}

template <class G>
G cos_from(const trig_arg<G>& a) noexcept {
    switch (a.q) {
        case 0:  return sin_reduced(a.rc);
        case 1:  return G::zero() - sin_reduced(a.r);
        case 2:  return G::zero() - sin_reduced(a.rc);
        default: return sin_reduced(a.r);
    }
}

template <class F, class G>
F trig_result(const G& v) noexcept {
    const bool neg = v.is_negative();
    return signed_root(fmath::float_cast<F>(neg ? (G::zero() - v) : v), neg);
}

template <class F>
F sin_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined())                 return F::undefined();
    if (x.is_nan() || x.is_infinite())    return F::nan();
    if (x.is_zero())                      return x;
    const trig_arg<G> a = trig_reduce(x);
    if (!a.ok) return F::nan();
    const G s = sin_from(a);
    return trig_result<F>(a.neg ? (G::zero() - s) : s);
}

template <class F>
F cos_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined())                 return F::undefined();
    if (x.is_nan() || x.is_infinite())    return F::nan();
    if (x.is_zero())                      return F(1);
    const trig_arg<G> a = trig_reduce(x);
    if (!a.ok) return F::nan();
    return trig_result<F>(cos_from(a));                 
}

template <class F>
F tan_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined())                 return F::undefined();
    if (x.is_nan() || x.is_infinite())    return F::nan();
    if (x.is_zero())                      return x;
    const trig_arg<G> a = trig_reduce(x);
    if (!a.ok) return F::nan();
    const G s = sin_from(a), c = cos_from(a);
    if (c.is_zero()) return inf_signed<F>(s.is_negative() != a.neg);
    const G t = s / c;
    return trig_result<F>(a.neg ? (G::zero() - t) : t);
}

template <class F>
F cot_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined())                 return F::undefined();
    if (x.is_nan() || x.is_infinite())    return F::nan();
    if (x.is_zero())                      return inf_signed<F>(x.is_negative());
    const trig_arg<G> a = trig_reduce(x);
    if (!a.ok) return F::nan();
    const G s = sin_from(a), c = cos_from(a);
    if (s.is_zero()) return inf_signed<F>(c.is_negative() != a.neg);
    const G t = c / s;
    return trig_result<F>(a.neg ? (G::zero() - t) : t);
}

template <class F>
F csc_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined())                 return F::undefined();
    if (x.is_nan() || x.is_infinite())    return F::nan();
    if (x.is_zero())                      return inf_signed<F>(x.is_negative());
    const trig_arg<G> a = trig_reduce(x);
    if (!a.ok) return F::nan();
    const G s = sin_from(a);
    if (s.is_zero()) return inf_signed<F>(a.neg);
    const G r = s.reciprocal();
    return trig_result<F>(a.neg ? (G::zero() - r) : r);
}

template <class F>
F sec_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined())                 return F::undefined();
    if (x.is_nan() || x.is_infinite())    return F::nan();
    if (x.is_zero())                      return F(1);
    const trig_arg<G> a = trig_reduce(x);
    if (!a.ok) return F::nan();
    const G c = cos_from(a);
    if (c.is_zero()) return F::infinity();
    return trig_result<F>(c.reciprocal());
}

} // namespace edetail

namespace tdetail {

template <class T> T native_sin(T v) noexcept { return std::sin(v); }
template <class T> T native_cos(T v) noexcept { return std::cos(v); }
template <class T> T native_tan(T v) noexcept { return std::tan(v); }
template <class T> T native_csc(T v) noexcept { return T(1) / std::sin(v); }
template <class T> T native_sec(T v) noexcept { return T(1) / std::cos(v); }
template <class T> T native_cot(T v) noexcept { return T(1) / std::tan(v); }

} // namespace tdetail

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_NORMAL_TRIG_DETAIL_HPP