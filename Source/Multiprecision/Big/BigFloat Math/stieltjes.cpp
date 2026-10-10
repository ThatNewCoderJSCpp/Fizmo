#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace constants {
namespace detail {

void sg_add(sg_int& a, const sg_int& b) {
    if (b.mag.is_zero()) return;
    if (a.mag.is_zero()) { a = b; return; }
    if (a.neg == b.neg)  { a.mag.add_mutable(b.mag); return; }
    const int c = a.mag.compare(b.mag);
    if (c == 0) { a.mag = BigUInt::zero(); a.neg = false; return; }

    if (c > 0) {
        a.mag.sub_mutable(b.mag);
    } else {
        BigUInt t = b.mag;
        t.sub_mutable(a.mag);
        a.mag = std::move(t);
        a.neg = b.neg;
    }
}

sg_int sg_scaled(const sg_int& a, std::uint64_t s, bool flip) {
    sg_int r;
    if (s == 0 || a.mag.is_zero()) return r;
    r.mag = a.mag;
    r.mag.mul_small_mutable(s);
    r.neg = flip ? !a.neg : a.neg;
    return r;
}

void sg_poly_step(std::vector<sg_int>& c, std::uint64_t m1) {
    const std::size_t d = c.size();
    std::vector<sg_int> out(d);

    for (std::size_t i = 0; i < d; ++i) {
        sg_int t = sg_scaled(c[i], m1, true);                    
        if (i + 1 < d) sg_add(t, sg_scaled(c[i + 1], static_cast<std::uint64_t>(i + 1), false));
        out[i] = std::move(t);
    }

    c.swap(out);
}

BigFloat sg_poly_eval(const std::vector<sg_int>& c, const BigFloat& L, const BigFloatContext& wc) {
    BigFloat r = BigFloat::zero();

    for (std::size_t i = c.size(); i > 0; --i) {
        r = BigFloat::mul(r, L, wc);
        const sg_int& t = c[i - 1];
        if (!t.mag.is_zero()) r = BigFloat::add(r, BigFloat(t.mag, t.neg, 0), wc);
    }

    return r;
}

BigFloat sg_powi(const BigFloat& b, std::uint64_t e, const BigFloatContext& wc) {
    BigFloat r = BigFloat::one();
    BigFloat t = b;

    while (e != 0) {
        if (e & 1u) r = BigFloat::mul(r, t, wc);
        e >>= 1;
        if (e != 0) t = BigFloat::mul(t, t, wc);
    }

    return r;
}

std::size_t sg_tail_estimate(std::size_t n, std::size_t N, std::size_t w, std::size_t jcap) {
    const double ln2  = 0.6931471805599453;
    const double lnN  = std::log(static_cast<double>(N));
    const double c    = std::log(2.0 * 3.141592653589793 * static_cast<double>(N));
    const double stop = -static_cast<double>(w) - 8.0;
    double prev = std::numeric_limits<double>::infinity();

    for (std::size_t j = 1; j <= jcap; ++j) {
        const double x  = 2.0 * static_cast<double>(j);
        const double lt = (ln2 - x * c + std::lgamma(x) + static_cast<double>(n) * std::log(lnN + std::log(x) + 1.0)) / ln2;
        if (lt < stop || lt >= prev) return j;
        prev = lt;
    }

    return jcap;
}

BigFloat sg_once(std::size_t n, std::size_t w, std::size_t& cancelled) {
    const BigFloatContext wc(BigFloatContext::clamp_precision(w), RoundingMode::nearest_even);
    std::size_t N = w / 3 + n + 12;
    if (N < 8)    N = 8;
    if (N > 1u << 20) N = 1u << 20;
    const BigFloat Nf(static_cast<std::uint64_t>(N));
    const BigFloat L = math::ln(Nf, wc);
    BigFloat S = BigFloat::zero();

    for (std::size_t k = 1; k < N; ++k) {
        BigFloat t;

        if (n == 0) {
            t = BigFloat(static_cast<std::uint64_t>(k)).reciprocal(wc);
        } else {
            if (k == 1) continue;                                   
            const BigFloat lk = math::ln(BigFloat(static_cast<std::uint64_t>(k)), wc);
            t = sg_powi(lk, static_cast<std::uint64_t>(n), wc);
            t = BigFloat::div(t, BigFloat(static_cast<std::uint64_t>(k)), wc);
        }

        S = BigFloat::add(S, t, wc);
    }

    const BigFloat Ln  = sg_powi(L, static_cast<std::uint64_t>(n), wc);
    const BigFloat mid = BigFloat::div(Ln, Nf, wc).scaled_pow2(-1);
    BigFloat       itg = BigFloat::mul(Ln, L, wc);
    itg = BigFloat::div(itg, BigFloat(static_cast<std::uint64_t>(n + 1)), wc);
    BigFloat main = BigFloat::add(S, mid, wc);
    main = BigFloat::sub(main, itg, wc);
    const std::int64_t eS = S.get_exp_base2();
    const std::int64_t eI = itg.get_exp_base2();
    const std::int64_t hi = (eS == BigFloat::exp_none) ? eI : ((eI == BigFloat::exp_none) ? eS : ((eS > eI) ? eS : eI));
    const std::int64_t em = main.get_exp_base2();
    cancelled = (hi != BigFloat::exp_none && em != BigFloat::exp_none && hi > em) ? static_cast<std::size_t>(hi - em) : 0;
    std::vector<sg_int> poly(n + 1);
    poly[n].mag = BigUInt::one();
    std::uint64_t m = 0;
    const BigFloat N2f  = BigFloat::mul(Nf, Nf, wc);
    BigFloat       Npow = N2f;                        
    BigFloat       fact = BigFloat::one();            
    BigFloat       tail = BigFloat::zero();
    std::int64_t   prev = std::numeric_limits<std::int64_t>::max();
    const std::size_t jcap = 4 * N + 32;
    tangent_reserve(sg_tail_estimate(n, N, w, jcap));

    for (std::size_t j = 1; j <= jcap; ++j) {
        while (m < 2 * j - 1) { sg_poly_step(poly, m + 1); ++m; }
        fact = BigFloat::mul(fact, BigFloat(static_cast<std::uint64_t>((2 * j - 1) * (2 * j))), wc);
        const BigFloat b = bernoulli_float(2 * j, wc);
        if (!b.is_finite()) break;
        BigFloat term = BigFloat::div(b, fact, wc);
        term = BigFloat::mul(term, sg_poly_eval(poly, L, wc), wc);
        term = BigFloat::div(term, Npow, wc);
        if (!term.is_finite()) break;
        const std::int64_t et = term.get_exp_base2();
        if (et == BigFloat::exp_none) break;
        if (et >= prev) break;
        if (et < em - static_cast<std::int64_t>(w)) { tail = BigFloat::add(tail, term, wc); break; }
        prev = et;
        tail = BigFloat::add(tail, term, wc);
        Npow = BigFloat::mul(Npow, N2f, wc);
    }

    return BigFloat::sub(main, tail, wc);
}

} // namespace detail
} // namespace constants
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace constants {

BigFloat stieltjes(std::size_t n, const BigFloatContext& ctx) {
    if (n == 0) return euler_mascheroni(ctx);
    if (n > detail::sg_order_cap) return BigFloat::undefined();
    std::size_t guard = 64;
    std::size_t extra = 0;

    for (;;) {
        std::size_t    can = 0;
        const BigFloat v   = detail::sg_once(n, ctx.precision + guard + extra, can);
        if (!v.is_finite()) return v;

        if (can > extra) {                                        
            if (detail::sg_guard_exhausted(ctx.precision, guard + extra)) return v.rounded(ctx);
            extra = can + 16;
            continue;
        }

        if (v.is_zero()) return v;
        const std::size_t want = ctx.precision + guard + extra;
        const std::size_t acc  = (want > extra + 16) ? (want - extra - 16) : 1;
        const std::size_t L    = v.significand().bit_length();
        const std::size_t err  = (L > acc) ? (L - acc) : 0;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, err + 2)) return v.rounded(ctx);
        if (detail::sg_guard_exhausted(ctx.precision, guard + extra)) return v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace constants
} // namespace multiprecision
} // namespace fizmo
