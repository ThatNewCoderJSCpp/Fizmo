#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace aydetail {

double ay_l2lo(const BF& x) {                                   
    if (!x.is_finite()) return ay_inf;
    if (x.is_zero())    return -ay_inf;
    return static_cast<double>(x.get_exp_base2());
}

double ay_lsum(double a, double b) {
    if (a == -ay_inf) return b;
    if (b == -ay_inf) return a;
    if (a < b) std::swap(a, b);
    return a + std::log2(1.0 + std::exp2(b - a));
}

bool ay_safe(const BF& v, std::size_t want, double lost, std::size_t prec) {
    if (v.is_zero() || !v.is_finite()) return false;
    BigUInt           sig = v.significand();
    const std::size_t L   = sig.bit_length();
    if (L < want) sig.shift_left_mutable(want - L);                    
    const double e = std::ceil(lost);
    if (!(e + 4.0 < static_cast<double>(sig.bit_length()))) return false;
    return constants::bfdetail::round_is_safe(sig, prec, (e <= 0.0) ? 2 : static_cast<std::size_t>(e) + 2);
}

const ay_consts& ay_constants(std::size_t want) {
    static thread_local ay_consts K;

    if (K.prec < want + 16) {
        const std::size_t p     = want + 64;
        const BFC         c     = ay_ctx(p);
        const BF          three = BF(static_cast<std::uint64_t>(3));
        const BF          g13   = gamma(BF::div(BF::one(), three, c), c);
        const BF          l3    = ln(three, c);
        const BF          p16   = exp(-BF::div(l3, BF(static_cast<std::uint64_t>(6)), c), c);
        const BF          p13   = exp(-BF::div(l3, three, c), c);
        K.c1   = BF::div(BF::mul(p16, g13, c), constants::two_pi(c), c);
        K.c2   = BF::div(p13, g13, c);
        K.prec = p;
    }

    return K;
}

ay_ser ay_series(const BF& x, bool need_h, std::size_t want) {
    const BFC    wc  = ay_ctx(want);
    const BFC    ac  = ay_ctx(want + 40);
    const BF     x3  = BF::mul(BF::mul(x, x, wc), x, wc);
    const double lx3 = x.is_zero() ? -ay_inf : 3.0 * gmdetail::gm_log2_fine(x);
    ay_ser s;
    BF t = BF::one();
    BF u = x;
    BF v = need_h ? BF::mul(x, x, wc).scaled_pow2(-1) : BF::zero();
    s.f = t;
    s.g = u;
    s.h = v;
    s.E = ay_l2hi(v);                                                  
    double sabs = ay_lsum(0.0, ay_lsum(ay_l2hi(u), ay_l2hi(v)));
    const std::uint64_t cap = 1ull << 26;

    for (std::uint64_t m = 0; m < cap; ++m) {
        const std::uint64_t a = 3 * m;
        t = BF::div(BF::mul(t, x3, wc), BF((a + 2) * (a + 3)), wc);
        u = BF::div(BF::mul(u, x3, wc), BF((a + 3) * (a + 4)), wc);
        if (need_h) v = BF::div(BF::mul(v, x3, wc), BF((a + 4) * (a + 5)), wc);
        s.f = BF::add(s.f, t, ac);
        s.g = BF::add(s.g, u, ac);
        if (need_h) s.h = BF::add(s.h, v, ac);

        const double lt = std::max(ay_l2hi(t), std::max(ay_l2hi(u), need_h ? ay_l2hi(v) : -ay_inf));
        s.E  = ay_lsum(s.E, lt + std::log2(3.0 * (7.0 * static_cast<double>(m) + 12.0)));  
        sabs = ay_lsum(sabs, lt + 1.585);

        const double rho = std::exp2(lx3 - std::log2(static_cast<double>(a + 5) * static_cast<double>(a + 6)));  

        if (rho < 0.5) {
            const double tail = lt + std::log2(6.0 * rho);                                    
            if (lt == -ay_inf || tail <= sabs - static_cast<double>(want) - 8.0) {
                if (lt != -ay_inf) s.E = ay_lsum(s.E, tail + static_cast<double>(want));
                s.ok = true;
                return s;
            }
        }
    }

    return s;
}

ay_val ay_from_series(ay_kind k, const BF& x, std::size_t want) {
    const bool   need_h = (k == ay_kind::gi || k == ay_kind::hi);
    const ay_ser s      = ay_series(x, need_h, want);
    if (!s.ok) return ay_undef();
    const BFC        wc  = ay_ctx(want);
    const ay_consts& K   = ay_constants(want);
    const BF         c1f = BF::mul(K.c1, s.f, wc);
    const BF         c2g = BF::mul(K.c2, s.g, wc);
    const BF         sum = (k == ay_kind::ai) ? BF::sub(c1f, c2g, wc) : BF::add(c1f, c2g, wc);
    double lparts = ay_lsum(ay_l2hi(c1f), ay_l2hi(c2g));
    BF     v;

    switch (k) {
        case ay_kind::ai:
            v = sum;
            break;

        case ay_kind::bi:
            v = BF::mul(constants::sqrt3(wc), sum, wc);
            lparts += 0.8;
            break;

        case ay_kind::hi: {
            const BF hp = BF::mul(constants::inv_pi(wc), s.h, wc);
            v      = BF::add(BF::mul(constants::inv_sqrt3(wc).scaled_pow2(1), sum, wc), hp, wc);
            lparts = ay_lsum(lparts + 0.21, ay_l2hi(hp));
            break;
        }

        default: {
            const BF hp = BF::mul(constants::inv_pi(wc), s.h, wc);
            v      = BF::sub(BF::mul(constants::inv_sqrt3(wc), sum, wc), hp, wc);
            lparts = ay_lsum(lparts, ay_l2hi(hp));
            break;
        }
    }

    if (v.is_zero()) return ay_val{v, 1.0e9};
    const double lv    = ay_l2lo(v);
    const double units = std::exp2(lparts + 3.0 - lv) + std::exp2(s.E + 1.0 - lv) + 2.0;
    return ay_val{v, std::log2(units) + 1.0};
}

bool ay_u_available(double zeta, double target) {
    if (!(zeta > 1.0)) return false;
    const double lz = std::log2(zeta);
    double lw = 0.0;

    for (std::uint64_t k = 1; k < 1000000; ++k) {
        const double kd = static_cast<double>(k);
        const double lr = std::log2((6 * kd - 5) * (6 * kd - 3) * (6 * kd - 1)) - std::log2((2 * kd - 1) * 216.0 * kd) - lz;
        if (lr >= 0.0) return false;
        lw += lr;
        if (lw <= -target) return true;
    }

    return false;
}

bool ay_w_available(double ly, double target, bool gi_bound) {
    double lw = gi_bound ? 1.0 : 0.0;

    for (std::uint64_t k = 1; k < 1000000; ++k) {
        const double kd = static_cast<double>(k);
        const double lr = std::log2((3 * kd - 2) * (3 * kd - 1)) - 3.0 * ly + (gi_bound ? 3.0 : 0.0);
        if (lr >= 0.0) return false;
        lw += lr;
        if (lw <= -target) return true;
    }

    return false;
}

BF ay_u_next(const BF& w, const BF& iz, std::uint64_t k, const BFC& wc) {
    return BF::div(BF::mul(BF::mul(w, iz, wc), BF((6 * k - 5) * (6 * k - 3) * (6 * k - 1)), wc), BF((2 * k - 1) * 216 * k), wc);
}

ay_val ay_ai_pos_asym(const BF& x, std::size_t want) {
    const BFC    wc   = ay_ctx(want);
    const BFC    ac   = ay_ctx(want + 32);
    const BF     sx   = sqrt(x, wc);
    const BF     zeta = BF::div(BF::mul(x, sx, wc).scaled_pow2(1), BF(static_cast<std::uint64_t>(3)), wc);
    const BF     iz   = zeta.reciprocal(wc);
    const double zd   = std::exp2(gmdetail::gm_log2_fine(zeta));
    BF     w = BF::one();
    BF     S = BF::one();
    double E = -ay_inf, tail = -ay_inf;

    for (std::uint64_t k = 1; ; ++k) {
        if (k > 1000000) return ay_undef();
        w = ay_u_next(w, iz, k, wc);
        const double lw = ay_l2hi(w);
        if (lw <= -static_cast<double>(want) - 8.0) { tail = lw + 1.0; break; }            
        S = (k & 1u) ? BF::sub(S, w, ac) : BF::add(S, w, ac);
        E = ay_lsum(E, lw + std::log2(4.0 * static_cast<double>(k) + 4.0));
    }

    const BF pre = BF::div(BF::mul(exp(-zeta, wc), constants::inv_sqrt_pi(wc), wc).scaled_pow2(-1), sqrt(sx, wc), wc);
    const BF v   = BF::mul(pre, S, wc);
    const double units = 4.0 * zd + 12.0 + std::exp2(E + 1.0) + std::exp2(tail + static_cast<double>(want));
    return ay_val{v, std::log2(units) + 1.0};
}

ay_val ay_neg_asym(const BF& y, bool bi, std::size_t want) {
    const BFC    wc   = ay_ctx(want);
    const BFC    ac   = ay_ctx(want + 32);
    const BF     sy   = sqrt(y, wc);
    const BF     zeta = BF::div(BF::mul(y, sy, wc).scaled_pow2(1), BF(static_cast<std::uint64_t>(3)), wc);
    const BF     iz   = zeta.reciprocal(wc);
    const double zd   = std::exp2(gmdetail::gm_log2_fine(zeta));
    BF     w = BF::one();
    BF     P = BF::one();
    BF     Q = BF::zero();
    double E = -ay_inf, tail = -ay_inf;

    for (std::uint64_t j = 1; ; ++j) {
        if (j > 1000000) return ay_undef();
        w = ay_u_next(w, iz, j, wc);
        const double lw = ay_l2hi(w);
        if (lw <= -static_cast<double>(want) - 8.0) { tail = lw + 1.0; break; }           
        const bool minus = ((j / 2) & 1u) != 0;
        if (j & 1u) Q = minus ? BF::sub(Q, w, ac) : BF::add(Q, w, ac);
        else        P = minus ? BF::sub(P, w, ac) : BF::add(P, w, ac);
        E = ay_lsum(E, lw + std::log2(4.0 * static_cast<double>(j) + 4.0));
    }

    const BF theta = BF::sub(zeta, constants::pi(wc).scaled_pow2(-2), wc);
    const BF c     = cos(theta, wc);
    const BF s     = sin(theta, wc);
    const BF core  = bi ? BF::sub(BF::mul(c, Q, wc), BF::mul(s, P, wc), wc)
                        : BF::add(BF::mul(c, P, wc), BF::mul(s, Q, wc), wc);
    const BF pre   = BF::div(constants::inv_sqrt_pi(wc), sqrt(sy, wc), wc);
    const BF v     = BF::mul(pre, core, wc);
    if (v.is_zero()) return ay_val{v, 1.0e9};
    const double lPQ   = ay_lsum(ay_l2hi(P), ay_l2hi(Q));
    const double labs  = ay_lsum(ay_lsum(lPQ + std::log2(3.0 * zd + 8.0), E + 1.0), tail + static_cast<double>(want));  
    const double units = std::exp2(ay_l2hi(pre) + labs - ay_l2lo(v)) + 3.0;
    return ay_val{v, std::log2(units) + 1.0};
}

ay_val ay_hi_neg_asym(const BF& y, std::size_t want) {
    const BFC wc  = ay_ctx(want);
    const BFC ac  = ay_ctx(want + 32);
    const BF  iy3 = BF::mul(BF::mul(y, y, wc), y, wc).reciprocal(wc);
    BF     w = BF::one();
    BF     S = BF::one();
    double E = -ay_inf, tail = -ay_inf;

    for (std::uint64_t k = 1; ; ++k) {
        if (k > 1000000) return ay_undef();
        w = BF::mul(BF::mul(w, iy3, wc), BF((3 * k - 2) * (3 * k - 1)), wc);
        const double lw = ay_l2hi(w);
        if (lw <= -static_cast<double>(want) - 8.0) { tail = lw + 1.0; break; }
        S = (k & 1u) ? BF::sub(S, w, ac) : BF::add(S, w, ac);
        E = ay_lsum(E, lw + std::log2(6.0 * static_cast<double>(k) + 6.0));
    }

    const BF v = BF::div(BF::mul(constants::inv_pi(wc), S, wc), y, wc);
    const double units = 6.0 + std::exp2(E + 1.0) + std::exp2(tail + static_cast<double>(want));
    return ay_val{v, std::log2(units) + 1.0};
}

ay_val ay_gi_pos_asym(const BF& x, std::size_t want) {
    const BFC wc  = ay_ctx(want);
    const BFC ac  = ay_ctx(want + 32);
    const BF  ix3 = BF::mul(BF::mul(x, x, wc), x, wc).reciprocal(wc);
    BF     w = BF::one();
    BF     S = BF::one();
    double E = -ay_inf, tail = -ay_inf;

    for (std::uint64_t k = 1; ; ++k) {
        if (k > 1000000) return ay_undef();
        w = BF::mul(BF::mul(w, ix3, wc), BF((3 * k - 2) * (3 * k - 1)), wc);
        const double lb = ay_l2hi(w) + 3.0 * static_cast<double>(k) + 1.0;
        if (lb <= -static_cast<double>(want) - 8.0) { tail = lb; break; }
        S = BF::add(S, w, ac);
        E = ay_lsum(E, ay_l2hi(w) + std::log2(6.0 * static_cast<double>(k) + 6.0));
    }

    const BF v = BF::div(BF::mul(constants::inv_pi(wc), S, wc), x, wc);
    const double units = 6.0 + std::exp2(E + 1.0) + std::exp2(tail + static_cast<double>(want));
    return ay_val{v, std::log2(units) + 1.0};
}

ay_regime ay_regime_of(const BF& x) {
    ay_regime r;
    r.neg  = x.signbit() && !x.is_zero();
    r.lx   = x.is_zero() ? -ay_inf : gmdetail::gm_log2_fine(x);
    r.zeta = x.is_zero() ? 0.0 : (2.0 / 3.0) * std::exp2(1.5 * r.lx);
    return r;
}

bool ay_use_asym(ay_kind k, const ay_regime& g, double target) {
    switch (k) {
        case ay_kind::ai: return ay_u_available(g.zeta, target);
        case ay_kind::bi: return g.neg && ay_u_available(g.zeta, target);
        case ay_kind::hi: return g.neg && ay_w_available(g.lx, target, false);
        default:          return g.neg ? (ay_u_available(g.zeta, target) && ay_w_available(g.lx, target, false))
                                       : ay_w_available(g.lx, target, true);
    }
}

ay_val ay_eval(ay_kind k, const BF& x, std::size_t want) {
    const ay_regime g = ay_regime_of(x);
    if (!ay_use_asym(k, g, static_cast<double>(want) + 8.0)) return ay_from_series(k, x, want);
    const BFC wc = ay_ctx(want);

    switch (k) {
        case ay_kind::ai: return g.neg ? ay_neg_asym(-x, false, want) : ay_ai_pos_asym(x, want);
        case ay_kind::bi: return ay_neg_asym(-x, true, want);
        case ay_kind::hi: return ay_hi_neg_asym(-x, want);

        default: {
            if (!g.neg) return ay_gi_pos_asym(x, want);
            const BF     y = -x;
            const ay_val b = ay_neg_asym(y, true, want);                                    
            if (!b.v.is_finite()) return b;
            const ay_val h = ay_hi_neg_asym(y, want);
            if (!h.v.is_finite()) return h;
            const BF v = BF::sub(b.v, h.v, wc);
            if (v.is_zero()) return ay_val{v, 1.0e9};
            const double lv    = ay_l2lo(v);
            const double units = std::exp2(ay_l2hi(b.v) + b.lost - lv) + std::exp2(ay_l2hi(h.v) + h.lost - lv) + 1.0;
            return ay_val{v, std::log2(units) + 1.0};
        }
    }
}

std::size_t ay_guard0(ay_kind k, const BF& x, std::size_t prec) {
    if (x.is_zero()) return 32;
    const ay_regime g     = ay_regime_of(x);
    double          extra = 2.0 * std::log2(g.zeta + 2.0);

    if (!ay_use_asym(k, g, static_cast<double>(prec) + 40.0 + extra)) {
        if (g.neg)                  extra += 1.4427 * g.zeta;                                    
        else if (k == ay_kind::ai)  extra += 2.8854 * g.zeta;                                    
        else if (k == ay_kind::gi)  extra += 1.4427 * g.zeta + std::log2(3.2 * std::exp2(g.lx) + 1.0);
    }

    return 32 + ((extra > 0.0 && extra < 1.0e7) ? static_cast<std::size_t>(extra) : 0);
}

BF ay_public(ay_kind k, const BF& x, const BFC& ctx) {
    if (x.is_nan())       return BF::nan();
    if (x.is_undefined()) return BF::undefined();

    if (x.is_infinite()) {
        if (x.signbit()) return BF::zero();                                                   
        return (k == ay_kind::ai || k == ay_kind::gi) ? BF::zero() : BF::infinity();
    }

    return ay_drive(ay_guard0(k, x, ctx.precision), ctx, [&](std::size_t want) { return ay_eval(k, x, want); });
}

} // namespace aydetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo
