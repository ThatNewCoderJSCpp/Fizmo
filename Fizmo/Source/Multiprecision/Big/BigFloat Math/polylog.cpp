#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace pldetail {

BigFloat pl_zeta_pos(std::uint64_t j, const BigFloatContext& wc) {                 
    return pl_cached(pl_zeta_cache(), static_cast<std::size_t>(j), wc, [j](const BigFloatContext& hc) { return riemann_zeta(BigFloat(j), hc); });
}

BigFloat pl_eta_pos(std::uint64_t j, const BigFloatContext& wc) {                  
    return pl_cached(pl_eta_cache(), static_cast<std::size_t>(j), wc, [j](const BigFloatContext& hc) { return dirichlet_eta(BigFloat(j), hc); });
}

BigFloat pl_zeta_negodd(std::uint64_t m, const BigFloatContext& wc) {              
    return pl_cached(pl_zneg_cache(), static_cast<std::size_t>(m), wc, [m](const BigFloatContext& hc) {
        return BigFloat::div(-constants::bernoulli_float(static_cast<std::size_t>(m + 1), hc), BigFloat(m + 1), hc);
    });
}

auto pl_acc::add(const BigFloat& t, double e, double sw) -> void {
        const double lt = hz_l2(t);
        err = hz_lsum(err, lt + std::log2(e));
        if (sw > 0.0) sens = hz_lsum(sens, lt + std::log2(sw));
        if (lt > lmax) lmax = lt;
    }

auto pl_acc::finish(const BigFloat& S, const BigFloatContext& wc, double ltail) const -> zt_val {
        const BigFloat v = S.rounded(wc);
        if (v.is_zero() || !v.is_finite()) return zt_val{v, 1.0e9};
        const double lv    = hz_l2(v);
        const double units = std::exp2(err - lv) + std::exp2(sens - lv) + std::exp2(ltail + static_cast<double>(wc.precision) - lv) + 1.0;
        return zt_val{v, std::log2(units) + 1.0};
    }

bool pl_neg_int(std::uint64_t m, const BigFloat& z, const BigFloatContext& ctx, BigFloat& out) {
    const BigFloat omz = hz_xadd(BigFloat::one(), z, true);
    if (omz.is_zero() || m > 4096) return false;
    auto span = [](const BigFloat& x) {
        return x.is_zero() ? 0.0 : static_cast<double>(x.significand().bit_length()) + std::fabs(static_cast<double>(x.exponent()));
    };
    const double bits = (static_cast<double>(m) + 1.0) * (span(z) + span(omz)) + static_cast<double>(m) * std::log2(static_cast<double>(m) + 2.0);
    if (bits > 8.0 * static_cast<double>(ctx.precision) + 16384.0) return false;
    BigFloat P = z;

    if (m >= 1) {
        const std::vector<BigUInt>& row = constants::detail::eulerian_half_row(static_cast<std::size_t>(m));
        BigFloat h = BigFloat::zero();
        for (std::size_t i = static_cast<std::size_t>(m); i-- > 0; ) h = hz_xadd(hz_xmul(h, z), BigFloat(constants::detail::en_at(row, static_cast<std::size_t>(m), i)));
        P = hz_xmul(z, h);
    }

    BigFloat D = BigFloat::one();
    BigFloat b = omz;

    for (std::uint64_t e = m + 1; e != 0; e >>= 1) {
        if (e & 1u) D = hz_xmul(D, b);
        if (e > 1)  b = hz_xmul(b, b);
    }

    out = BigFloat::div(P, D, ctx);
    return true;
}

zt_val pl_int_direct(std::uint64_t n, const BigFloat& z, std::size_t want) {
    const double lz = hz_l2(z);
    const double lq = std::log2(1.0 - std::exp2(lz));
    const double nd = static_cast<double>(n);
    const double wd = static_cast<double>(want);
    std::uint64_t N = 2;

    for (;; ++N) {
        if (N > hzdetail::lp_term_cap) return zt_val{BigFloat::undefined(), 0.0};
        if (static_cast<double>(N) * lz - nd * std::log2(static_cast<double>(N)) - lq <= lz - wd - 4.0) break;
    }

    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloatContext ac(BigFloatContext::clamp_precision(want + ztdetail::zt_bitlen(N) + 8), RoundingMode::nearest_even);
    pl_acc   acc;
    BigFloat S  = BigFloat::zero();
    BigFloat zk = z;

    for (std::uint64_t k = 1; k < N; ++k) {
        if (k > 1) zk = BigFloat::mul(zk, z, wc);
        const BigFloat t = BigFloat::div(zk, BigFloat(hz_upow(BigUInt(k), n)), wc);
        S = BigFloat::add(S, t, ac);
        acc.add(t, static_cast<double>(k) + 1.0, 0.0);
    }

    return acc.finish(S, wc, static_cast<double>(N) * lz - nd * std::log2(static_cast<double>(N)) - lq);
}

zt_val pl_int_mu(std::uint64_t n, const BigFloat& mu, bool eta, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloatContext ac(BigFloatContext::clamp_precision(want + 16), RoundingMode::nearest_even);
    const double am   = std::fabs(ztdetail::zt_to_double(mu));
    const double R    = eta ? pl_pi : pl_2pi;
    const double l2am = std::log2(am);
    const double lR   = std::log2(R);
    if (!(am < 0.98 * R)) return zt_val{BigFloat::undefined(), 0.0};
    const double lpre = 2.0 + static_cast<double>(n) * l2am - lR - std::lgamma(static_cast<double>(n) + 1.0) / hz_ln2 - std::log2(1.0 - am / R);
    const std::uint64_t kcap = 8 * static_cast<std::uint64_t>(want) + n + 1000;
    const BigFloat half = BigFloat::one().scaled_pow2(-1);
    pl_acc   acc;
    BigFloat S  = BigFloat::zero();
    BigFloat pk = BigFloat::one();                                                      

    for (std::uint64_t k = 0; k <= kcap; ++k) {
        if (k > 0) pk = BigFloat::div(BigFloat::mul(pk, mu, wc), BigFloat(k), wc);
        BigFloat coef;
        double   ce   = 0.0;
        bool     have = true;

        if (k + 2 <= n) {
            coef = eta ? pl_eta_pos(n - k, wc) : pl_zeta_pos(n - k, wc);
            ce   = 2.0;
        } else if (k + 1 == n) {
            if (eta) {
                coef = constants::ln2(wc);
                ce   = 1.0;
            } else {
                BigFloat H = BigFloat::zero();
                for (std::uint64_t j = 1; j < n; ++j) H = BigFloat::add(H, BigFloat(j).reciprocal(ac), ac);
                coef = BigFloat::sub(H, ln(-mu, wc), wc);
                ce   = 4.0;
                acc.sens = hz_lsum(acc.sens, hz_l2(pk));                                
            }
        } else if (k == n) {
            coef = eta ? half : -half;                                                  
        } else {
            const std::uint64_t m = k - n;

            if (m % 2 == 0) {
                have = false;
            } else {
                coef = pl_zeta_negodd(m, wc);
                ce   = 2.0;

                if (eta) {                                                              
                    BigUInt f = constants::bfdetail::unit(static_cast<std::size_t>(m + 1));
                    f.sub_small_mutable(1);
                    coef = -BigFloat::mul(coef, BigFloat(f), wc);
                    ce   = 3.0;
                }
            }
        }

        if (have) {
            BigFloat t = BigFloat::mul(coef, pk, wc);
            if (eta) t = -t;
            S = BigFloat::add(S, t, ac);
            acc.add(t, ce + 3.0 * static_cast<double>(k) + 1.0, static_cast<double>(k));
        }

        if (k > n && (k - n) % 2 == 1) {
            const double ltail = lpre + static_cast<double>(k - n + 1) * (l2am - lR);
            if (ltail <= acc.lmax - static_cast<double>(want) - 4.0) return acc.finish(S, wc, ltail);
        }
    }

    return zt_val{BigFloat::undefined(), 0.0};
}

zt_val pl_int_raw(std::uint64_t n, const BigFloat& z, std::size_t want) {
    const double lz = hz_l2(z);
    if (lz <= -1.0) return pl_int_direct(n, z, want);
    const double am  = -lz * hz_ln2;
    const double R   = z.signbit() ? pl_pi : pl_2pi;
    const double kmu = (static_cast<double>(want) + 4.0) / std::log2(R / am) + static_cast<double>(n);
    const double lq  = std::log2(1.0 - std::exp2(lz));
    double Nd = 2.0;
    while (Nd <= kmu && !(Nd * lz - static_cast<double>(n) * std::log2(Nd) - lq <= lz - static_cast<double>(want) - 4.0)) Nd += 1.0;
    if (Nd <= kmu) return pl_int_direct(n, z, want);
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    return pl_int_mu(n, ln(z.abs(), wc), z.signbit(), want);
}

zt_val pl_int_inversion(std::uint64_t n, const BigFloat& z, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloatContext ac(BigFloatContext::clamp_precision(want + 16), RoundingMode::nearest_even);
    const BigFloat L  = ln(-z, wc);
    const BigFloat w  = z.reciprocal(wc);
    const zt_val   in = pl_int_raw(n, w, want);
    if (!in.v.is_finite()) return in;
    pl_acc   acc;
    BigFloat S = (n % 2 == 0) ? -in.v : in.v;
    acc.err  = hz_l2(in.v) + in.lost;
    acc.sens = 1.0;        
    BigFloat q = BigFloat::one();                                                        

    for (std::uint64_t j = 0; j <= n; ++j) {
        if (j > 0) q = BigFloat::div(BigFloat::mul(q, L, wc), BigFloat(j), wc);
        if ((n - j) % 2 != 0) continue;
        const std::uint64_t kk = (n - j) / 2;
        const BigFloat      t  = (kk == 0) ? -q : -BigFloat::mul(pl_eta_pos(2 * kk, wc), q, wc).scaled_pow2(1);
        S = BigFloat::add(S, t, ac);
        acc.add(t, 3.0 * static_cast<double>(j) + 4.0, 0.0);
    }

    return acc.finish(S, wc, hz_ninf());
}

zt_val pl_nonint_mu(const BigFloat& s, const BigFloat& om, const BigFloat& mu, bool eta, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloatContext ac(BigFloatContext::clamp_precision(want + 16), RoundingMode::nearest_even);
    const double sd = ztdetail::zt_to_double(s);
    const double am = std::fabs(ztdetail::zt_to_double(mu));
    if (!(am < 0.98 * (eta ? pl_pi : pl_2pi))) return zt_val{BigFloat::undefined(), 0.0};
    const BigFloat one  = BigFloat::one();
    const BigFloat half = one.scaled_pow2(-1);
    pl_acc   acc;
    BigFloat S = BigFloat::zero();

    if (!eta) {
        const BigFloat g = gamma(om, wc);
        if (!g.is_finite()) return zt_val{g, 0.0};
        const BigFloat sing = BigFloat::mul(g, exp(BigFloat::mul(-om, ln(-mu, wc), wc), wc), wc);
        S = sing;
        acc.add(sing, 4.0 + 3.0 * std::fabs(1.0 - sd) * (std::fabs(std::log(am)) + 1.0), std::fabs(1.0 - sd));
    }

    const BigFloat      tp   = constants::two_pi(wc);
    const BigFloat      i4p2 = BigFloat::mul(tp, tp, wc).reciprocal(wc);
    const std::uint64_t kcap = 4 * static_cast<std::uint64_t>(want) + 1000;
    BigFloat      pk = one;
    BigFloat      chi[2];
    double        crel[2] = {0.0, 0.0};
    bool          chain   = false;
    std::uint64_t ka      = 0;
    double        lt_prev = hz_ninf();

    for (std::uint64_t k = 0; k <= kcap; ++k) {
        if (k > 0) pk = BigFloat::div(BigFloat::mul(pk, mu, wc), BigFloat(k), wc);
        const BigFloat sig = hz_xadd(s, BigFloat(k), true);                             
        const BigFloat oms = hz_xadd(om, BigFloat(k));                                  
        BigFloat coef;
        double   ce;

        if (!chain && BigFloat::compare(sig, half) == BigFloat::ordering::greater) {
            const zt_val c = eta ? dldetail::dl_alt(sig, false, want) : ztdetail::zt_core(sig, oms, want);
            if (!c.v.is_finite()) return c;
            coef = c.v;
            ce   = std::exp2(c.lost);
        } else {
            const std::size_t par = static_cast<std::size_t>(k & 1u);

            if (!chain) {
                chain = true;
                ka    = k;
                const zt_val c0 = ztdetail::zt_chi_low(sig, oms, want);
                const zt_val c1 = ztdetail::zt_chi_low(hz_xadd(sig, one, true), hz_xadd(oms, one), want);
                if (!c0.v.is_finite()) return c0;
                if (!c1.v.is_finite()) return c1;
                chi[par]      = c0.v;  crel[par]      = std::exp2(c0.lost);
                chi[par ^ 1u] = c1.v;  crel[par ^ 1u] = std::exp2(c1.lost);
            } else if (k >= ka + 2) {
                const BigFloat f = BigFloat::mul(hz_xadd(BigFloat(k - 1), s, true), hz_xadd(BigFloat(k), s, true), wc);
                chi[par]   = -BigFloat::mul(BigFloat::mul(chi[par], f, wc), i4p2, wc);
                crel[par] += 6.0;
            }

            const zt_val z1 = ztdetail::zt_core(oms, sig, want);                        
            if (!z1.v.is_finite()) return z1;
            coef = BigFloat::mul(chi[par], z1.v, wc);
            ce   = crel[par] + std::exp2(z1.lost) + 1.0;

            if (eta) {                                                                  
                coef = BigFloat::mul(coef, dldetail::dl_omp2(oms, wc), wc);
                ce  += dldetail::dl_omp2_units(oms) + 1.0;
            }
        }

        BigFloat t = BigFloat::mul(coef, pk, wc);
        if (eta) t = -t;
        S = BigFloat::add(S, t, ac);
        acc.add(t, ce + 3.0 * static_cast<double>(k) + 1.0, static_cast<double>(k));
        const double lt = hz_l2(t);

        if (chain && k >= ka + 1) {                                                      
            const double j   = static_cast<double>(k - 1);
            const double F   = std::max(1.0, ((j + 1.0 - sd) * (j + 2.0 - sd)) / ((j + 1.0) * (j + 2.0)));
            const double rho = eta ? 4.0 + 3.0 / (std::exp2(1.0 - (sd - j)) - 1.0) : 1.0;
            const double q   = (am / pl_2pi) * (am / pl_2pi) * F * rho;

            if (q < 1.0) {
                const double ltail = hz_lsum(lt_prev, lt) + std::log2(q) - std::log2(1.0 - q);
                if (ltail <= acc.lmax - static_cast<double>(want) - 4.0) return acc.finish(S, wc, ltail);
            }
        }

        lt_prev = lt;
    }

    return zt_val{BigFloat::undefined(), 0.0};
}

zt_val pl_raw_unit(const BigFloat& z, const BigFloat& s, const BigFloat& om, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloat        one = BigFloat::one();
    std::int64_t k = 0;

    if (hzdetail::hz_int64(s, k)) {
        if (k <= 0) {
            BigFloat out;
            if (pl_neg_int(static_cast<std::uint64_t>(-k), z, wc, out)) return zt_val{out, 1.0};
            const zt_val r = hzdetail::lp_direct(ln(z.abs(), wc), z.signbit(), s, one, want);
            if (!r.v.is_finite()) return r;
            return zt_val{BigFloat::mul(z, r.v, wc), std::log2(std::exp2(r.lost) + 1.0)};
        }

        if (k == 1) return zt_val{-ln(hz_xadd(one, z, true), wc), 1.0};
        return pl_int_raw(static_cast<std::uint64_t>(k), z, want);
    }

    const double   lz  = hz_l2(z);
    const bool     neg = z.signbit();
    const BigFloat L   = ln(z.abs(), wc);

    if (lz <= -1.0 || (neg && !s.signbit())) {
        const zt_val r = (lz <= -1.0) ? hzdetail::lp_direct(L, neg, s, one, want) : hzdetail::lp_alt(L, s, one, want);
        if (!r.v.is_finite()) return r;
        return zt_val{BigFloat::mul(z, r.v, wc), std::log2(std::exp2(r.lost) + 1.0)};
    }

    return pl_nonint_mu(s, om, L, neg, want);
}

zt_val pl_li2_raw(const BigFloat& z, std::size_t want) {
    const BigFloatContext    wc(want, RoundingMode::nearest_even);
    const BigFloat::ordering ca = BigFloat::compare(z.abs(), BigFloat::one());
    if (ca == BigFloat::ordering::less)  return pl_int_raw(2, z, want);
    if (ca == BigFloat::ordering::equal) return zt_val{-constants::zeta2(wc).scaled_pow2(-1), 1.0};    // Li_2(-1) = -pi^2/12
    const double mu = hz_l2(z) * hz_ln2;                                                                // ln(-z) > 0
    if (mu <= 1.0) return pl_int_mu(2, ln(-z, wc), true, want);
    return pl_int_inversion(2, z, want);
}

} // namespace pldetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat polylog(const BigFloat& z, const BigFloat& s, const BigFloatContext& ctx) {
    using namespace pldetail;
    using hzdetail::hz_drive;
    if (s.is_nan()       || z.is_nan())       return BigFloat::nan();
    if (s.is_undefined() || z.is_undefined()) return BigFloat::undefined();
    if (s.is_infinite()) return s.signbit() ? BigFloat::undefined() : z.rounded(ctx);    
    if (z.is_zero())     return z;
    if (z.is_infinite()) return (z.signbit() && !s.signbit() && !s.is_zero()) ? BigFloat::infinity(true) : BigFloat::undefined();
    const BigFloat           one = BigFloat::one();
    const BigFloat::ordering c1  = BigFloat::compare(z, one);
    const BigFloat::ordering ca  = BigFloat::compare(z.abs(), one);
    std::int64_t k    = 0;
    const bool   sint = hzdetail::hz_int64(s, k);

    if (sint && k <= 0) {                                                                
        const std::uint64_t m = static_cast<std::uint64_t>(-k);
        if (c1 == BigFloat::ordering::equal) return (m % 2 == 1) ? BigFloat::infinity() : BigFloat::undefined();  // pole of order m+1
        BigFloat out;
        if (pl_neg_int(m, z, ctx, out)) return out;
        if (ca != BigFloat::ordering::less) return BigFloat::undefined();

        return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) -> zt_val {
            const BigFloatContext wc(want, RoundingMode::nearest_even);
            const zt_val r = hzdetail::lp_direct(ln(z.abs(), wc), z.signbit(), s, one, want);
            if (!r.v.is_finite()) return r;
            return zt_val{BigFloat::mul(z, r.v, wc), std::log2(std::exp2(r.lost) + 1.0)};
        });
    }

    if (c1 == BigFloat::ordering::greater) return BigFloat::nan();                       

    if (sint && k == 1) {                                                                
        if (c1 == BigFloat::ordering::equal) return BigFloat::infinity();
        const BigFloat omz = hz_xadd(one, z, true);
        return hz_drive(32, ctx, [&](std::size_t want) -> zt_val {
            return zt_val{-ln(omz, BigFloatContext(want, RoundingMode::nearest_even)), 1.0};
        });
    }

    if (c1 == BigFloat::ordering::equal) {                                               
        return (BigFloat::compare(s, one) == BigFloat::ordering::greater) ? riemann_zeta(s, ctx) : BigFloat::infinity();
    }

    const bool     spos = !s.signbit();
    const BigFloat om   = ztdetail::zt_one_minus(s);

    if (ca == BigFloat::ordering::equal) {                                               
        return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) -> zt_val {
            zt_val r = spos ? dldetail::dl_alt(s, false, want) : dldetail::dl_eta_fe(s, om, want);
            r.v = -r.v;
            return r;
        });
    }

    if (sint) {                                                                          
        const std::uint64_t n = static_cast<std::uint64_t>(k);
        if (ca == BigFloat::ordering::less) return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) { return pl_int_raw(n, z, want); });
        if (n > 65536) return BigFloat::undefined();
        const double mu = hz_l2(z) * hz_ln2;                                             

        if (mu <= 1.0) {
            return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) {
                return pl_int_mu(n, ln(-z, BigFloatContext(want, RoundingMode::nearest_even)), true, want);
            });
        }

        return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) { return pl_int_inversion(n, z, want); });
    }

    if (ca == BigFloat::ordering::less) {                                                
        const double lz  = hz_l2(z);
        const bool   neg = z.signbit();
        return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) -> zt_val {
            const BigFloatContext wc(want, RoundingMode::nearest_even);
            const BigFloat        L = ln(z.abs(), wc);

            if (lz <= -1.0 || (neg && spos)) {
                const zt_val r = (lz <= -1.0) ? hzdetail::lp_direct(L, neg, s, one, want) : hzdetail::lp_alt(L, s, one, want);
                if (!r.v.is_finite()) return r;
                return zt_val{BigFloat::mul(z, r.v, wc), std::log2(std::exp2(r.lost) + 1.0)};
            }

            return pl_nonint_mu(s, om, L, neg, want);
        });
    }

    return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) {                
        return pl_nonint_mu(s, om, ln(-z, BigFloatContext(want, RoundingMode::nearest_even)), true, want);
    });
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
