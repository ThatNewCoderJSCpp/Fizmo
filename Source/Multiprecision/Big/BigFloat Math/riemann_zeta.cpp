#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace ztdetail {

double zt_to_double(const BigFloat& x) {
    if (x.is_zero()) return 0.0;
    const double m = std::exp2(gmdetail::gm_log2_fine(x));
    return x.signbit() ? -m : m;
}

BigFloat zt_one_minus(const BigFloat& x) {                      
    if (x.is_zero()) return BigFloat::one();
    const std::int64_t e   = x.get_exp_base2();
    const std::int64_t top = (e > 0) ? e : 0;
    const std::int64_t lsb = (x.exponent() < 0) ? x.exponent() : 0;
    const std::size_t  p   = BigFloatContext::clamp_precision(static_cast<std::size_t>(top - lsb) + 3);
    return BigFloat::sub(BigFloat::one(), x, BigFloatContext(p, RoundingMode::nearest_even));
}

void zt_reduce_half(const BigFloat& y, BigFloat& f, bool& flip) {
    const BigInt q = y.get_integer_part();
    f    = y.get_fractional_part();
    flip = q.is_odd();
    if (f.is_zero()) return;
    if (BigFloat::compare(f.abs(), BigFloat::one().scaled_pow2(-1)) != BigFloat::ordering::greater) return;
    const BigFloatContext ec(BigFloatContext::clamp_precision(static_cast<std::size_t>(-f.exponent()) + 4), RoundingMode::nearest_even);
    f    = f.signbit() ? BigFloat::add(f, BigFloat::one(), ec) : BigFloat::sub(f, BigFloat::one(), ec);
    flip = !flip;
}

BigFloat zt_sticky(const BigFloat& base, bool neg, std::size_t gap, const BigFloatContext& ctx) {
    const BigFloatContext hc(BigFloatContext::clamp_precision(gap + 16), RoundingMode::nearest_even);
    return BigFloat::add(base, BigFloat::one(neg).scaled_pow2(-static_cast<std::int64_t>(gap)), hc).rounded(ctx);
}

bool zt_safe(const BigFloat& v, std::size_t want, double lost, std::size_t prec) {
    if (v.is_zero()) return false;
    BigUInt           sig = v.significand();
    const std::size_t L   = sig.bit_length();
    if (L < want) sig.shift_left_mutable(want - L);                     
    const double e = std::ceil(lost) + ((L > want) ? static_cast<double>(L - want) : 0.0);
    if (!(e + 4.0 < static_cast<double>(sig.bit_length()))) return false;
    const std::size_t err = (e <= 0.0) ? 0 : static_cast<std::size_t>(e);
    return multiprecision::constants::bfdetail::round_is_safe(sig, prec, err + 2);
}

std::size_t zt_guard0(const BigFloat& s) {
    const std::int64_t e = s.get_exp_base2();
    return 32 + ((e > 0 && e < 4096) ? static_cast<std::size_t>(e) : 0);
}

std::vector<BigFloat> zt_powers_exp(const BigFloat& s, std::uint64_t M, const BigFloatContext& wc) {
    std::vector<BigFloat>      pw(static_cast<std::size_t>(M) + 1, BigFloat::one());
    std::vector<std::uint64_t> spf(static_cast<std::size_t>(M) + 1, 0);

    for (std::uint64_t k = 2; k <= M; ++k) {
        if (spf[k] == 0) {
            for (std::uint64_t j = k; j <= M; j += k) if (spf[j] == 0) spf[j] = k;
        }

        if (spf[k] == k) pw[k] = exp(-BigFloat::mul(s, ln(BigFloat(k), wc), wc), wc);
        else             pw[k] = BigFloat::mul(pw[spf[k]], pw[k / spf[k]], wc);
    }

    return pw;
}

BigUInt zt_one_minus_pow(std::uint64_t m, unsigned q, std::uint64_t u, std::uint64_t D, std::size_t W) {
    using multiprecision::constants::bfdetail::unit;
    BigUInt t = unit(W);
    t.mul_small_mutable(m);
    t.shift_right_mutable(q);
    t.mul_small_mutable(u);
    t.div_small_mutable(D);
    BigUInt acc = t;
    BigUInt v;
    const bool fuse = D < (std::uint64_t(1) << 40);

    for (std::uint64_t j = 2; !t.is_zero(); ++j) {
        v = t;
        v.mul_small_mutable(m);
        v.shift_right_mutable(q);
        t.mul_small_mutable(j - 1);
        t.sub_mutable(v);
        if (u != 1) t.mul_small_mutable(u);
        if (fuse) t.div_small_mutable(j * D);
        else { t.div_small_mutable(j); t.div_small_mutable(D); }
        acc.add_mutable(t);
    }

    BigUInt r = unit(W);
    r.sub_mutable(acc);
    return r;
}

BigUInt zt_one_minus_pow_x(std::uint64_t m, unsigned q, unsigned e, std::uint64_t D, std::size_t W) {
    using multiprecision::constants::bfdetail::unit;
    BigUInt t = unit(W + e);
    t.mul_small_mutable(m);
    t.shift_right_mutable(q);
    t.div_small_mutable(D);
    BigUInt             acc  = t;
    const std::uint64_t jcap = ~std::uint64_t(0) / D;

    for (std::uint64_t j = 2; !t.is_zero(); ++j) {
        t.mul_small_mutable(((j - 1) << q) - m);
        if (e >= q) t.shift_left_mutable(e - q); else t.shift_right_mutable(q - e);
        if (j <= jcap) t.div_small_mutable(j * D);
        else { t.div_small_mutable(j); t.div_small_mutable(D); }
        acc.add_mutable(t);
    }

    BigUInt r = unit(W);
    r.sub_mutable(acc);
    return r;
}

BigUInt zt_one_minus_pow_short(std::uint64_t m, unsigned q, std::uint64_t D, std::size_t W) {
    return zt_one_minus_pow_x(m, q, 0, D, W);
}

BigUInt zt_atanh_pow2_fixed(unsigned e, std::uint64_t v, std::size_t W) {
    using multiprecision::constants::bfdetail::unit;
    BigUInt term = unit(W + e);
    term.div_small_mutable(v);
    BigUInt sum = term;

    for (std::uint64_t k = 3; ; k += 2) {
        term.shift_left_mutable(2 * e);
        term.div_small_mutable(v);
        term.div_small_mutable(v);
        if (term.is_zero()) break;
        BigUInt t = term;
        t.div_small_mutable(k);
        if (t.is_zero()) break;
        sum.add_mutable(t);
    }

    return sum;
}

bool zt_short_fraction(const BigUInt& ffix, std::size_t W, unsigned qmax, std::uint64_t& m, unsigned& q, BigUInt& fl) {
    using multiprecision::constants::bfdetail::unit;
    BigUInt hi = ffix;
    hi.shift_right_mutable(W - qmax);
    m  = hi.is_zero() ? 0 : hi.get_lowest_bits();
    q  = qmax;
    hi.shift_left_mutable(W - qmax);
    fl = ffix;
    fl.sub_mutable(hi);
    while (m != 0 && (m & 1u) == 0 && q > 0) { m >>= 1; --q; }
    return fl.is_zero();
}

BigUInt zt_exp_small_neg(const BigUInt& y, std::size_t W) {
    using multiprecision::constants::bfdetail::unit;
    using multiprecision::constants::bfdetail::fx_mul;
    BigUInt pos = unit(W);
    BigUInt neg = BigUInt::zero();
    BigUInt t   = unit(W);

    for (std::uint64_t k = 1; ; ++k) {
        t = fx_mul(t, y, W);
        t.div_small_mutable(k);
        if (t.is_zero()) break;
        if (k & 1u) neg.add_mutable(t); else pos.add_mutable(t);
    }

    pos.sub_mutable(neg);
    return pos;
}

bool zt_split(const BigFloat& s, std::size_t W, std::int64_t& n, BigUInt& ffix) {
    using multiprecision::constants::bfdetail::unit;
    if (!s.is_finite()) return false;
    ffix = BigUInt::zero();
    if (s.is_zero()) { n = 0; return true; }
    const BigUInt&     M = s.significand();
    const std::int64_t E = s.exponent();

    if (E >= 0) {
        if (static_cast<std::int64_t>(M.bit_length()) + E > 40) return false;
        n = static_cast<std::int64_t>(M.get_lowest_bits() << E);
        if (s.signbit()) n = -n;
        return true;
    }

    const std::size_t e  = static_cast<std::size_t>(-E);
    BigUInt           ip = M;
    ip.shift_right_mutable(e);
    if (ip.bit_length() > 40) return false;
    BigUInt fr = M;
    BigUInt mask = unit(e);
    mask.sub_small_mutable(1);
    fr.and_mutable(mask);
    n = static_cast<std::int64_t>(ip.is_zero() ? 0 : ip.get_lowest_bits());
    if (e <= W) fr.shift_left_mutable(W - e); else fr.shift_right_mutable(e - W);

    if (!s.signbit()) {
        ffix = std::move(fr);
        return true;
    }

    n = -n;
    if (fr.is_zero()) return true;
    n -= 1;
    ffix = unit(W);
    ffix.sub_mutable(fr);
    return true;
}

std::vector<BigFloat> zt_powers(const BigFloat& s, std::uint64_t M, const BigFloatContext& wc) {
    using multiprecision::constants::bfdetail::unit;
    using multiprecision::constants::bfdetail::fx_mul;
    using multiprecision::constants::bfdetail::atanh_inv_fixed;
    using multiprecision::constants::bfdetail::ln2_fixed;
    const std::size_t W = wc.precision + 2 * zt_bitlen(M) + zt_bitlen(wc.precision) + 24;
    std::int64_t      n = 0;
    BigUInt           ffix;
    if (M < 2 || M >= (std::uint64_t(1) << 32) || !zt_split(s, W, n, ffix)) return zt_powers_exp(s, M, wc);
    const std::uint64_t an = static_cast<std::uint64_t>(n < 0 ? -n : n);
    if (static_cast<double>(an) * std::log2(static_cast<double>(M)) > 64.0 * static_cast<double>(wc.precision) + 4096.0) return zt_powers_exp(s, M, wc);

    std::vector<BigFloat>      pw(static_cast<std::size_t>(M) + 1, BigFloat::one());
    std::vector<std::uint64_t> spf(static_cast<std::size_t>(M) + 1, 0);
    const bool                 frac = !ffix.is_zero();
    std::uint64_t              m = 0;
    unsigned                   q = 62;
    BigUInt                    fl = BigUInt::zero();

    bool short_f = false;

    if (frac) {
        BigUInt lo = ffix;
        BigUInt msk = unit(W - 40);
        msk.sub_small_mutable(1);
        lo.and_mutable(msk);
        if (lo.is_zero()) {
            BigUInt hi = ffix;
            hi.shift_right_mutable(W - 40);
            m = hi.get_lowest_bits();
            q = 40;
            while ((m & 1u) == 0 && q > 0) { m >>= 1; --q; }
            short_f = true;
        }
    }

    if (frac && !short_f) {
        BigUInt hi = ffix;
        hi.shift_right_mutable(W - 62);
        m  = hi.is_zero() ? 0 : hi.get_lowest_bits();
        hi.shift_left_mutable(W - 62);
        fl = ffix;
        fl.sub_mutable(hi);
        while (m != 0 && (m & 1u) == 0 && q > 0) { m >>= 1; --q; }
    }

    const bool          use_lo = frac && !fl.is_zero();
    std::vector<BigUInt> P, L;
    if (frac) P.assign(static_cast<std::size_t>(M) + 1, unit(W));
    if (use_lo) L.assign(static_cast<std::size_t>(M) + 1, BigUInt::zero());

    for (std::uint64_t k = 2; k <= M; ++k) {
        if (spf[k] == 0) {
            for (std::uint64_t j = k; j <= M; j += k) if (spf[j] == 0) spf[j] = k;
        }

        const std::uint64_t a = spf[k], b = k / spf[k];

        if (a != k) {
            if (frac)   P[k] = fx_mul(P[a], P[b], W);
            if (use_lo) { L[k] = L[a]; L[k].add_mutable(L[b]); }
            pw[k] = BigFloat::mul(pw[a], pw[b], wc);
            continue;
        }

        BigFloat v = BigFloat::one();

        if (frac) {
            if (short_f)     P[k] = fx_mul(P[k - 1], zt_one_minus_pow_short(m, q, k, W), W);
            else if (m != 0) P[k] = fx_mul(P[k - 1], zt_one_minus_pow(m, q, 1, k, W), W);
            else        P[k] = P[k - 1];
            BigUInt pk = P[k];

            if (use_lo) {
                if (k == 2) L[k] = ln2_fixed(W);
                else {
                    BigUInt t = atanh_inv_fixed(2 * k - 1, W);
                    t.shift_left_mutable(1);
                    L[k] = L[k - 1];
                    L[k].add_mutable(t);
                }
                pk = fx_mul(pk, zt_exp_small_neg(fx_mul(fl, L[k], W), W), W);
            }

            v = BigFloat(std::move(pk), false, -static_cast<std::int64_t>(W));
        }

        if (n != 0) {
            const BigFloat kn(hzdetail::hz_upow(BigUInt(k), an));
            v = (n > 0) ? BigFloat::div(v, kn, wc) : BigFloat::mul(v, kn, wc);
        } else {
            v = v.rounded(wc);
        }

        pw[k] = std::move(v);
    }

    return pw;
}

void zt_borwein_step(BigUInt& c, std::uint64_t n, std::uint64_t i) {
    c.mul_small_mutable(2 * (n + i - 1));
    c.mul_small_mutable(n - i + 1);
    c.div_small_mutable(i * (2 * i - 1));
}

zt_val zt_core(const BigFloat& s, const BigFloat& om, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(want + 2))) != BigFloat::ordering::less) return zt_val{BigFloat::one(), 1.0};
    const double        sd = zt_to_double(s);
    const double        as = std::fabs(sd);
    const std::uint64_t nb = static_cast<std::uint64_t>(std::ceil((static_cast<double>(want) + 3.0) / 2.5431)) + 1;
    std::uint64_t K = 0;

    if (sd >= 2.0) {                                                    
        for (std::uint64_t k = 2; k <= nb / 2; ++k) {
            const double kd = static_cast<double>(k);
            if (-sd * std::log2(kd) + std::log2(1.0 + kd / (sd - 1.0)) <= -static_cast<double>(want) - 3.0) { K = k; break; }
        }
    }

    if (K != 0) {
        const std::uint64_t         M  = K - 1;
        const std::vector<BigFloat> pw = zt_powers(s, M, wc);
        const std::int64_t          F  = static_cast<std::int64_t>(want + zt_bitlen(M)) + 2;
        BigInt A(static_cast<std::int64_t>(0));
        for (std::uint64_t k = 1; k <= M; ++k) A.add_mutable(pw[k].scaled_pow2(F).get_integer_part());
        const double ce = 3.0 * as * std::log(static_cast<double>(M) + 1.0) + 2.0 * std::log2(static_cast<double>(M) + 1.0) + 3.0;
        return zt_val{BigFloat(A).scaled_pow2(-F).rounded(wc), std::log2(ce + 2.0) + 1.0};
    }

    const std::uint64_t         n  = nb;
    const std::vector<BigFloat> pw = zt_powers(s, n, wc);
    BigUInt c  = BigUInt::one();
    BigUInt dn = BigUInt::one();

    for (std::uint64_t i = 1; i <= n; ++i) {
        zt_borwein_step(c, n, i);
        dn.add_mutable(c);
    }

    const std::int64_t F = static_cast<std::int64_t>(want + zt_bitlen(n)) + 2 - (static_cast<std::int64_t>(dn.bit_length()) - 1);
    BigInt  A(static_cast<std::int64_t>(0));
    BigUInt dk = BigUInt::one();
    c = BigUInt::one();

    for (std::uint64_t k = 0; k < n; ++k) {
        BigUInt e = dn;
        e.sub_mutable(dk);
        const BigInt t = BigFloat::mul(BigFloat(e), pw[k + 1], wc).scaled_pow2(F).get_integer_part();
        if (k & 1u) A.sub_mutable(t); else A.add_mutable(t);
        zt_borwein_step(c, n, k + 1);
        dk.add_mutable(c);
    }

    const BigFloat h   = BigFloat::mul(om, multiprecision::constants::ln2(wc), wc).scaled_pow2(-1);
    const BigFloat em1 = (sd >= 1.5) ? BigFloat::sub(pw[2].scaled_pow2(1), BigFloat::one(), wc) : BigFloat::mul(sinh(h, wc), exp(h, wc), wc).scaled_pow2(1);
    const BigFloat q   = BigFloat::div(BigFloat(A).scaled_pow2(-F), BigFloat::mul(BigFloat(dn), em1, wc), wc);
    const double ce  = 3.0 * as * std::log(static_cast<double>(n) + 1.0) + 2.0 * std::log2(static_cast<double>(n) + 1.0) + 4.0;
    const double ah  = std::fabs(zt_to_double(h));
    const double tot = (0.25 + 2.0 * std::sqrt(static_cast<double>(n)) * ce + 1.0) / 0.6 + (6.0 + 5.0 * ah) + 3.0;
    return zt_val{-q, std::log2(tot) + 1.0};                         
}

zt_shift::zt_shift(const BigFloat& s, std::size_t want) : s_(s), want_(want), wb_(0), n_(0), k_(0) {
    const double as = std::fabs(zt_to_double(s));
    n_  = static_cast<std::uint64_t>(std::ceil((0.7625 * static_cast<double>(want + 40) + as + 8.0) / 6.283185307179586 * 1.15 + 8.0));
    wb_ = want + 40 + zt_bitlen(n_);
    const BigFloatContext wc(wb_, RoundingMode::nearest_even);
    inv_.assign(static_cast<std::size_t>(n_) + 1, BigFloat::zero());
    for (std::uint64_t n = 1; n <= n_; ++n) inv_[n] = BigFloat::div(BigFloat::one(), BigFloat(n), wc);
}

zt_val zt_shift::at(std::uint64_t k) {
    const BigFloatContext wc(wb_, RoundingMode::nearest_even);
    const BigFloat        one = BigFloat::one();
    const BigFloat        sig = hzdetail::hz_xadd(zt_one_minus(s_), BigFloat(k));
    if (!started_ || k < k_) {
        p_ = zt_powers(sig, n_, wc);
        started_ = true;
    } else {
        for (std::uint64_t j = k_; j < k; ++j) {
            for (std::uint64_t n = 2; n <= n_; ++n) p_[n] = BigFloat::mul(p_[n], inv_[n], wc);
        }
    }
    k_ = k;
    const double sd   = zt_to_double(sig);
    const double wd   = static_cast<double>(wb_);
    BigFloat     sum  = BigFloat::zero();
    bool         done = false;
    for (std::uint64_t n = 1; n < n_; ++n) {
        sum = BigFloat::add(sum, p_[n], wc);
        if (sd > 1.0) {
            const double m  = static_cast<double>(n + 1);
            const double tb = -sd * std::log2(m) + std::log2(1.0 + m / (sd - 1.0));
            if (tb < -wd) {
                done = true;
                break;
            }
        }
    }
    if (!done) {
        const BigFloat Nb(n_);
        const BigFloat pN  = p_[n_];
        const BigFloat sm1 = hzdetail::hz_xadd(sig, one, true);
        if (sm1.is_zero()) return zt_val{BigFloat::undefined(), 0.0};
        sum = BigFloat::add(sum, BigFloat::div(BigFloat::mul(pN, Nb, wc), sm1, wc), wc);
        sum = BigFloat::add(sum, pN.scaled_pow2(-1), wc);
        const BigFloat iN2  = BigFloat::mul(inv_[n_], inv_[n_], wc);
        BigFloat       poch = sig;
        BigFloat       qu   = BigFloat::mul(pN, inv_[n_], wc);
        double         prev = std::numeric_limits<double>::infinity();
        for (std::size_t j = 1;; ++j) {
            if (bern_.size() <= j) {
                if (bern_.empty()) bern_.push_back(BigFloat::zero());
                while (bern_.size() <= j) {
                    const std::size_t i = bern_.size();
                    BigFloat f = one;
                    for (std::uint64_t t = 2; t <= 2 * i; ++t) f = BigFloat::mul(f, BigFloat(t), BigFloatContext(wb_ + 16, RoundingMode::nearest_even));
                    bern_.push_back(BigFloat::div(multiprecision::constants::bernoulli(2 * i, BigFloatContext(wb_ + 8, RoundingMode::nearest_even)), f, wc));
                }
            }
            const BigFloat term = BigFloat::mul(BigFloat::mul(poch, qu, wc), bern_[j], wc);
            const double   lt   = term.is_zero() ? -std::numeric_limits<double>::infinity() : gmdetail::gm_log2_fine(term.abs());
            if (lt < -wd - 2.0) break;
            if (j > 3 && lt > prev) return zt_val{BigFloat::undefined(), 0.0};
            prev = lt;
            sum  = BigFloat::add(sum, term, wc);
            poch = BigFloat::mul(BigFloat::mul(poch, hzdetail::hz_xadd(sig, BigFloat(static_cast<std::uint64_t>(2 * j - 1))), wc), hzdetail::hz_xadd(sig, BigFloat(static_cast<std::uint64_t>(2 * j))), wc);
            qu   = BigFloat::mul(qu, iN2, wc);
            if (j > 100000) return zt_val{BigFloat::undefined(), 0.0};
        }
    }
    return zt_val{sum.rounded(BigFloatContext(want_, RoundingMode::nearest_even)), 2.0};
}

zt_val zt_chi_low(const BigFloat& s, const BigFloat& om, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    BigFloat f;
    bool     flip = false;
    zt_reduce_half(s.scaled_pow2(-1), f, flip);
    if (f.is_zero()) return zt_val{BigFloat::zero(), 0.0};
    const BigFloat sn = sin(BigFloat::mul(multiprecision::constants::pi(wc), f, wc), wc);
    const BigFloat pw = exp(BigFloat::mul(s, gmdetail::gm_half_ln_two_pi(wc), wc).scaled_pow2(1), wc);
    const BigFloat g  = gamma(om, wc);
    if (!pw.is_finite()) return zt_val{pw, 0.0};
    if (!g.is_finite())  return zt_val{g, 0.0};
    BigFloat v = BigFloat::mul(pw, multiprecision::constants::inv_pi(wc), wc);
    v = BigFloat::mul(v, sn, wc);
    v = BigFloat::mul(v, g, wc);
    const double a = std::fabs(zt_to_double(s)) * 1.8378770664093453;  
    return zt_val{flip ? -v : v, std::log2(10.0 + 3.0 * a) + 1.0};
}

zt_val zt_fe(const BigFloat& s, const BigFloat& om, std::size_t want) {          
    const zt_val c = zt_chi_low(s, om, want);
    if (!c.v.is_finite() || c.v.is_zero()) return c;
    const zt_val z = zt_core(om, s, want);
    if (!z.v.is_finite()) return z;
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    return zt_val{BigFloat::mul(c.v, z.v, wc), std::log2(std::exp2(c.lost) + std::exp2(z.lost) + 1.0) + 1.0};
}

bool zt_neg_int(const BigFloat& s, const BigFloatContext& ctx, BigFloat& out) {
    if (!s.signbit() || !s.get_fractional_part().is_zero()) return false;
    const BigInt k = s.get_integer_part();

    if (k.bit_length() > 62) {
        if (!k.is_even()) return false;
        out = BigFloat::zero();
        return true;
    }

    const std::uint64_t m = k.get_lowest_bits();
    if (m % 2 == 0) { out = BigFloat::zero(); return true; }
    const std::uint64_t n = (m + 1) / 2;
    if (n > multiprecision::constants::detail::bn_index_cap) return false;
    const BigUInt T = multiprecision::constants::tangent_number(n);
    if (T.is_undefined()) return false;
    BigUInt den = multiprecision::constants::bfdetail::unit(2 * n);
    den.sub_small_mutable(1);
    out = BigFloat::div(BigFloat(T, n % 2 == 1, -2 * static_cast<std::int64_t>(n)), BigFloat(den), ctx);   // sign before rounding
    return true;
}

} // namespace ztdetail
} // namespace math
} // namespace fizmo

namespace fizmo {
namespace math {

BigFloat riemann_zeta(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_indeterminate()) return BigFloat::indeterminate();
    if (s.is_infinite())  return s.signbit() ? BigFloat::undefined() : BigFloat::one();
    const BigFloat one  = BigFloat::one();
    const BigFloat half = one.scaled_pow2(-1);
    if (s.is_zero()) return -half;
    if (BigFloat::compare(s, one) == BigFloat::ordering::equal) return BigFloat::undefined();          // pole
    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(2))) == BigFloat::ordering::equal) return multiprecision::constants::zeta2(ctx);
    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(3))) == BigFloat::ordering::equal) return multiprecision::constants::apery(ctx);
    BigFloat out;
    if (ztdetail::zt_neg_int(s, ctx, out)) return out;

    std::int64_t ev = 0;
    if (hzdetail::hz_int64(s, ev) && ev >= 4 && ev % 2 == 0 && ev <= 4096 && static_cast<std::size_t>(ev) < ctx.precision + 2) {
        const std::size_t k = static_cast<std::size_t>(ev);
        const BigFloat    f(gmdetail::fact_range(1, k + 1));
        for (std::size_t guard = 32;; guard *= 2) {
            const std::size_t     want = BigFloatContext::clamp_precision(ctx.precision + guard);
            const BigFloatContext wc(want + 8, RoundingMode::nearest_even);
            const BigFloat b = multiprecision::constants::bernoulli(k, wc).abs();
            const BigFloat p = pow(multiprecision::constants::two_pi(wc), BigInt(static_cast<long long>(k)), wc);
            const BigFloat v = BigFloat::div(BigFloat::mul(b, p, wc), f, BigFloatContext(want, RoundingMode::nearest_even)).scaled_pow2(-1);
            const double lost = std::log2(static_cast<double>(k)) + 6.0;
            if (ztdetail::zt_safe(v, want, lost, ctx.precision) || ztdetail::zt_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        }
    }

    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(ctx.precision + 2))) != BigFloat::ordering::less) {
        return ztdetail::zt_sticky(one, false, ctx.precision + 4, ctx);
    }

    if (s.get_exp_base2() < -static_cast<std::int64_t>(ctx.precision) - 10) {
        return ztdetail::zt_sticky(-half, !s.signbit(), ctx.precision + 8, ctx);
    }

    const BigFloat om    = ztdetail::zt_one_minus(s);
    const bool     fe    = BigFloat::compare(s, half) == BigFloat::ordering::less;
    std::size_t    guard = ztdetail::zt_guard0(s);

    for (;;) {
        const std::size_t      want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const ztdetail::zt_val r    = fe ? ztdetail::zt_fe(s, om, want) : ztdetail::zt_core(s, om, want);
        if (!r.v.is_finite() || r.v.is_zero()) return r.v;
        if (ztdetail::zt_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return r.v.rounded(ctx);
        guard *= 2;
    }
}

BigFloat riemann_xi(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_indeterminate()) return BigFloat::indeterminate();
    if (s.is_infinite())  return BigFloat::infinity();
    const BigFloat one  = BigFloat::one();
    const BigFloat half = one.scaled_pow2(-1);
    const BigFloat t    = (BigFloat::compare(s, half) == BigFloat::ordering::less) ? ztdetail::zt_one_minus(s) : s;
    if (BigFloat::compare(t, one) == BigFloat::ordering::equal) return half;                          // xi(0) = xi(1) = 1/2
    const BigFloat om    = ztdetail::zt_one_minus(t);
    std::size_t    guard = ztdetail::zt_guard0(t);

    for (;;) {
        const std::size_t      want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext  wc(want, RoundingMode::nearest_even);
        const ztdetail::zt_val z = ztdetail::zt_core(t, om, want);
        if (!z.v.is_finite()) return z.v;
        const BigFloat pis = exp(-BigFloat::mul(t, gmdetail::gm_ln_pi(wc), wc).scaled_pow2(-1), wc);
        const BigFloat g   = gamma(t.scaled_pow2(-1), wc);
        if (!g.is_finite()) return g;
        BigFloat v = BigFloat::mul(t, om, wc);                                                        // = -s (s - 1)
        v = BigFloat::mul(v, pis, wc);
        v = BigFloat::mul(v, g, wc);
        v = BigFloat::mul(v, z.v, wc);
        v = (-v).scaled_pow2(-1);
        if (!v.is_finite()) return v;
        const double a    = std::fabs(ztdetail::zt_to_double(t)) * 0.5723649429247001;               // |s| ln(pi) / 2
        const double lost = std::log2(9.0 + 3.0 * a + std::exp2(z.lost)) + 1.0;
        if (ztdetail::zt_safe(v, want, lost, ctx.precision)) return v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

BigFloat riemann_chi(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_indeterminate()) return BigFloat::indeterminate();
    if (s.is_infinite())  return BigFloat::undefined();                                               // oscillates
    const BigFloat           half = BigFloat::one().scaled_pow2(-1);
    const BigFloat::ordering ch   = BigFloat::compare(s, half);
    if (ch == BigFloat::ordering::equal) return BigFloat::one();                                      // chi(1/2)^2 = 1
    const bool     low = ch == BigFloat::ordering::less;
    const BigFloat om  = ztdetail::zt_one_minus(s);
    const BigFloat a   = low ? s : om;                                                                // chi(s) = 1 / chi(1 - s)
    const BigFloat b   = low ? om : s;
    BigFloat f;
    bool     flip = false;
    ztdetail::zt_reduce_half(a.scaled_pow2(-1), f, flip);
    if (f.is_zero()) return low ? BigFloat::zero() : BigFloat::undefined();
    std::size_t guard = ztdetail::zt_guard0(s);

    for (;;) {
        const std::size_t      want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext  wc(want, RoundingMode::nearest_even);
        const ztdetail::zt_val c = ztdetail::zt_chi_low(a, b, want);
        if (!c.v.is_finite()) return low ? c.v : BigFloat::zero(c.v.signbit());
        const BigFloat v    = low ? c.v : c.v.reciprocal(wc);
        const double   lost = low ? c.lost : c.lost + 1.0;
        if (ztdetail::zt_safe(v, want, lost, ctx.precision)) return v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace math
} // namespace fizmo
